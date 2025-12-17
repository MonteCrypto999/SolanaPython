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
    if (!hexMatch) {
        console.log(`${desc}: COMPILE FAILED`);
        return;
    }
    const bytecode = Buffer.from(hexMatch[1], 'hex');

    const connection = new Connection(RPC_URL, 'confirmed');
    const pikaKeypairData = JSON.parse(fs.readFileSync(KEYPAIR_PATH, 'utf8'));
    const programId = Keypair.fromSecretKey(Uint8Array.from(pikaKeypairData)).publicKey;
    const payer = Keypair.generate();
    await connection.requestAirdrop(payer.publicKey, 1e9);
    await new Promise(r => setTimeout(r, 500));

    const data = Buffer.concat([Buffer.from([0x02]), bytecode]);
    const ix = new TransactionInstruction({ keys: [{ pubkey: payer.publicKey, isSigner: true, isWritable: true }], programId, data });
    const cu = ComputeBudgetProgram.setComputeUnitLimit({ units: 1400000 });
    const heap = ComputeBudgetProgram.requestHeapFrame({ bytes: 256 * 1024 });
    const tx = new Transaction().add(cu, heap, ix);

    try {
        await sendAndConfirmTransaction(connection, tx, [payer], { commitment: 'confirmed' });
        console.log(`${desc}: OK (${bytecode.length} bytes)`);
    } catch (e) {
        if (e.message.includes('Access violation')) {
            console.log(`${desc}: HEAP OVERFLOW`);
        } else {
            console.log(`${desc}: FAILED - ${e.message.slice(0,100)}`);
        }
    }
}

async function main() {
    console.log('Testing heap usage step by step...\n');

    await test('1+1', 'Step 0: Simple math');
    await test('import solana\n1', 'Step 1: import solana');
    await test('import solana\nimport struct\n1', 'Step 2: import struct');
    await test('import solana\nimport struct\nseeds = [b"vault"]\n1', 'Step 3: seeds list');
    await test('import solana\nimport struct\nseeds = [b"vault"]\npid = solana.program_id()\n1', 'Step 4: program_id()');
    await test('import solana\nimport struct\nseeds = [b"vault"]\npid = solana.program_id()\npda, bump = solana.find_program_address(seeds, pid)\n1', 'Step 5: find_program_address');
    await test('import solana\nimport struct\nseeds = [b"vault"]\npid = solana.program_id()\npda, bump = solana.find_program_address(seeds, pid)\ndata = struct.pack("<I", [0])\n1', 'Step 6: struct.pack');
    await test('import solana\nimport struct\nseeds = [b"vault"]\npid = solana.program_id()\npda, bump = solana.find_program_address(seeds, pid)\ndata = struct.pack("<I", [0])\ndata = data + struct.pack("<Q", [1000000])\n1', 'Step 7: bytes concat 1');
    await test('import solana\nimport struct\nseeds = [b"vault"]\npid = solana.program_id()\npda, bump = solana.find_program_address(seeds, pid)\ndata = struct.pack("<I", [0])\ndata = data + struct.pack("<Q", [1000000])\ndata = data + struct.pack("<Q", [64])\n1', 'Step 8: bytes concat 2');
    await test('import solana\nimport struct\nseeds = [b"vault"]\npid = solana.program_id()\npda, bump = solana.find_program_address(seeds, pid)\ndata = struct.pack("<I", [0])\ndata = data + struct.pack("<Q", [1000000])\ndata = data + struct.pack("<Q", [64])\ndata = data + pid\n1', 'Step 9: bytes concat 3');
    await test('import solana\nimport struct\nseeds = [b"vault"]\npid = solana.program_id()\npda, bump = solana.find_program_address(seeds, pid)\ndata = struct.pack("<I", [0])\ndata = data + struct.pack("<Q", [1000000])\ndata = data + struct.pack("<Q", [64])\ndata = data + pid\naccounts = [(0, 1, 1), (2, 1, 1)]\n1', 'Step 10: accounts list');
    await test('import solana\nimport struct\nseeds = [b"vault"]\npid = solana.program_id()\npda, bump = solana.find_program_address(seeds, pid)\ndata = struct.pack("<I", [0])\ndata = data + struct.pack("<Q", [1000000])\ndata = data + struct.pack("<Q", [64])\ndata = data + pid\naccounts = [(0, 1, 1), (2, 1, 1)]\nsigner_seeds = [[b"vault", bytes([bump])]]\n1', 'Step 11: signer_seeds');
}

main().catch(console.error);
