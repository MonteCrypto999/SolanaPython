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
    await new Promise(r => setTimeout(r, 1000));

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
            console.log(`${desc}: OK (no return)`);
        }
    } catch (e) {
        console.log(`${desc}: FAILED - ${e.message?.slice(0,80)}`);
    }
}

async function main() {
    await test('len(bytearray([1,2,3]))', 'bytearray create');
    await test(`data = bytearray([1,2,3])
data.extend([4,5,6])
len(data)`, 'bytearray extend len');
    await test(`data = bytearray([1,2,3])
data.append(4)
len(data)`, 'bytearray append len');
    await test(`data = bytearray(20)
len(data)`, 'bytearray preallocate');
    await test(`data = bytearray(52)
data[0] = 0
data[4] = 64
data[5] = 66
data[6] = 15
data[12] = 64
len(data)`, 'bytearray index assign');
}

main();
