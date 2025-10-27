#!/usr/bin/env node
/**
 * CU Benchmark - prints CU usage per test
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

const RPC_URL = 'http://localhost:8899';
const SOLANA_BUILD_DIR = path.join(__dirname, '..');
const PIKA_KEYPAIR_PATH = path.join(SOLANA_BUILD_DIR, 'pika-keypair.json');
const COMPILER_PATH = path.join(SOLANA_BUILD_DIR, 'tools', 'pika_compile');
const MODE_EXECUTE_BYTECODE = 0x02;

// Benchmark test cases - variety of operations
const BENCHMARKS = [
    { name: 'empty', code: `0` },
    { name: 'int_add', code: `1+2+3+4+5` },
    { name: 'int_mul', code: `2*3*4*5` },
    { name: 'float_add', code: `1.1+2.2+3.3` },
    { name: 'float_mul', code: `2.5*3.5*4.5` },
    { name: 'string_short', code: `'hello'` },
    { name: 'string_concat', code: `'hello' + ' ' + 'world'` },
    { name: 'list_create', code: `[1,2,3,4,5]` },
    { name: 'list_access', code: `x=[1,2,3,4,5]\nx[2]` },
    { name: 'range_5', code: `s=0\nfor i in range(5):\n    s=s+i\ns` },
    { name: 'range_10', code: `s=0\nfor i in range(10):\n    s=s+i\ns` },
    { name: 'range_20', code: `s=0\nfor i in range(20):\n    s=s+i\ns` },
    { name: 'func_call', code: `def f(x):\n    return x*2\nf(21)` },
    { name: 'func_recursive', code: `def f(n):\n    if n<=1:\n        return 1\n    return n*f(n-1)\nf(5)` },
    { name: 'math_sqrt', code: `import math\nmath.sqrt(16.0)` },
    { name: 'math_sin', code: `import math\nmath.sin(1.0)` },
    { name: 'math_multi', code: `import math\nmath.sqrt(math.sin(1.0)**2 + math.cos(1.0)**2)` },
    { name: 'bytearray_create', code: `bytearray(10)` },
    { name: 'bytearray_list', code: `bytearray([1,2,3,4,5])` },
    { name: 'class_simple', code: `class C:\n    x=42\nc=C()\nc.x` },
    { name: 'abs_builtin', code: `abs(-42)` },
    { name: 'len_builtin', code: `len([1,2,3,4,5])` },
    { name: 'max_builtin', code: `max(1,2,3,4,5)` },
];

async function compileBytecode(code) {
    const result = spawnSync(COMPILER_PATH, [code], { encoding: 'utf8', maxBuffer: 10 * 1024 * 1024 });
    if (result.status !== 0) return null;
    let hex = result.stdout.trim();
    if (hex.startsWith('0x')) hex = hex.slice(2);
    return Buffer.from(hex, 'hex');
}

async function runBenchmark(connection, programId, payer, name, code) {
    const bytecode = await compileBytecode(code);
    if (!bytecode || bytecode.length === 0) {
        console.log(`  ${name}: compile failed`);
        return null;
    }

    const instructionData = Buffer.concat([Buffer.from([MODE_EXECUTE_BYTECODE]), bytecode]);
    const instruction = new TransactionInstruction({
        keys: [],
        programId,
        data: instructionData,
    });

    const tx = new Transaction();
    tx.add(ComputeBudgetProgram.setComputeUnitLimit({ units: 1_000_000 }));
    tx.add(ComputeBudgetProgram.requestHeapFrame({ bytes: 256 * 1024 }));
    tx.add(instruction);

    try {
        const sig = await sendAndConfirmTransaction(connection, tx, [payer], { commitment: 'confirmed' });
        const txInfo = await connection.getTransaction(sig, { commitment: 'confirmed', maxSupportedTransactionVersion: 0 });
        const cu = txInfo?.meta?.computeUnitsConsumed || 0;
        return { name, cu, bytes: bytecode.length };
    } catch (e) {
        // Check for logs
        const logs = e.logs || [];
        const errorLog = logs.find(l => l.includes('Program log:')) || e.message.substring(0, 80);
        console.log(`  ${name}: ${errorLog}`);
        return { name, cu: -1, error: e.message };
    }
}

async function main() {
    if (!fs.existsSync(COMPILER_PATH)) {
        console.log('Error: Compiler not found. Run: cd tools && ./build_compiler.sh');
        process.exit(1);
    }

    const connection = new Connection(RPC_URL, 'confirmed');
    const pikaKeypair = JSON.parse(fs.readFileSync(PIKA_KEYPAIR_PATH, 'utf8'));
    const programId = Keypair.fromSecretKey(Uint8Array.from(pikaKeypair)).publicKey;
    const payer = Keypair.generate();

    // Airdrop
    const sig = await connection.requestAirdrop(payer.publicKey, 10_000_000_000);
    await connection.confirmTransaction(sig);

    console.log('CU Benchmark');
    console.log('============');
    console.log('');
    console.log('Test                    CU       Bytes');
    console.log('----                    --       -----');

    const results = [];
    for (const bench of BENCHMARKS) {
        const result = await runBenchmark(connection, programId, payer, bench.name, bench.code);
        if (result && result.cu > 0) {
            console.log(`${result.name.padEnd(20)}  ${String(result.cu).padStart(8)}  ${String(result.bytes).padStart(6)}`);
            results.push(result);
        } else {
            console.log(`${bench.name.padEnd(20)}  FAILED`);
        }
    }

    console.log('');
    console.log('Summary');
    console.log('-------');
    const totalCU = results.reduce((s, r) => s + r.cu, 0);
    const avgCU = Math.round(totalCU / results.length);
    console.log(`Tests: ${results.length}`);
    console.log(`Total CU: ${totalCU}`);
    console.log(`Avg CU: ${avgCU}`);
}

main().catch(console.error);
