#!/usr/bin/env node
const { spawnSync } = require('child_process');
const { Connection, Keypair, Transaction, TransactionInstruction, sendAndConfirmTransaction, ComputeBudgetProgram } = require('@solana/web3.js');
const fs = require('fs');
const path = require('path');

const RPC_URL = 'http://localhost:8899';
const COMPILER_PATH = path.join(__dirname, '..', 'tools', 'pika_compile');
const KEYPAIR_PATH = path.join(__dirname, '..', 'pika-keypair.json');

async function test(code, desc) {
    const result = spawnSync(COMPILER_PATH, [code], { encoding: 'utf8' });
    const hexMatch = result.stdout.match(/0x([0-9a-f]+)/i);
    if (!hexMatch) { console.log(`${desc}: COMPILE FAIL`); return; }
    const bytecode = Buffer.from(hexMatch[1], 'hex');

    const connection = new Connection(RPC_URL, 'confirmed');
    const pikaKeypairData = JSON.parse(fs.readFileSync(KEYPAIR_PATH, 'utf8'));
    const programId = Keypair.fromSecretKey(Uint8Array.from(pikaKeypairData)).publicKey;
    const payer = Keypair.generate();
    await connection.requestAirdrop(payer.publicKey, 1e9);
    await new Promise(r => setTimeout(r, 800));

    const instrData = Buffer.concat([Buffer.from([0x02]), bytecode]);
    const ix = new TransactionInstruction({ keys: [{ pubkey: payer.publicKey, isSigner: true, isWritable: true }], programId, data: instrData });
    const cu = ComputeBudgetProgram.setComputeUnitLimit({ units: 1400000 });
    const heap = ComputeBudgetProgram.requestHeapFrame({ bytes: 256 * 1024 });
    const tx = new Transaction().add(cu, heap, ix);

    try {
        const sig = await sendAndConfirmTransaction(connection, tx, [payer], { commitment: 'confirmed' });
        const txDetails = await connection.getTransaction(sig, { commitment: 'confirmed', maxSupportedTransactionVersion: 0 });
        if (txDetails?.meta?.returnData?.data) {
            const ret = Buffer.from(txDetails.meta.returnData.data[0], 'base64').toString('utf8');
            console.log(`${desc}: ${ret}`);
        } else {
            console.log(`${desc}: (no return)`);
        }
    } catch (e) {
        const errLogs = e.logs?.filter(l => l.includes('Program log'));
        console.log(`${desc}: FAIL`);
        if (errLogs?.length) console.log('  Logs:', errLogs.slice(-3));
    }
}

async function main() {
    console.log('=== Type checks ===');
    await test('import struct\ntype(struct.pack("<I", [0]))', 'struct.pack type');
    await test('import solana\ntype(solana.program_id())', 'program_id type');
    await test('type(b"hello")', 'bytes literal type');

    console.log('\n=== Concat tests ===');
    await test('b"a" + b"b"', 'bytes + bytes');
    await test('import struct\nstruct.pack("<I", [0]) + struct.pack("<I", [1])', 'struct + struct');
    await test('import solana\nb"hello" + solana.program_id()', 'bytes + program_id');
    await test('import struct\nimport solana\nstruct.pack("<I", [0]) + solana.program_id()', 'struct + program_id');
}

main();
