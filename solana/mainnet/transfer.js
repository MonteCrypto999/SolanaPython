/**
 * Transfer SOL using PikaPython CPI
 *
 * This script demonstrates transferring SOL from your wallet to a recipient
 * using a PikaPython script executed on-chain.
 *
 * Usage:
 *   node transfer.js <program_id> <recipient_pubkey> [amount_sol]
 *
 * Example:
 *   node transfer.js Abc123... RecipientPubkey123... 0.001
 */

const {
    Connection,
    Keypair,
    PublicKey,
    Transaction,
    TransactionInstruction,
    ComputeBudgetProgram,
    SystemProgram,
    sendAndConfirmTransaction,
} = require('@solana/web3.js');
const fs = require('fs');
const path = require('path');
const { execSync } = require('child_process');

// Configuration - defaults to mainnet
const DEFAULT_RPC_URL = 'https://api.mainnet-beta.solana.com';
const DEFAULT_KEYPAIR_PATH = `${process.env.HOME}/.config/solana/id.json`;
const COMPILER_PATH = path.join(__dirname, '../tools/pika_compile');

const EXECUTE_BYTECODE = 0x02;

/**
 * Compile Python to bytecode
 */
function compile(code) {
    const result = execSync(`${COMPILER_PATH} "${code.replace(/"/g, '\\"')}"`, {
        encoding: 'utf8',
        maxBuffer: 10 * 1024 * 1024,
    });
    // Compiler outputs: "0x<hex>\nBytecode size: N bytes"
    const lines = result.trim().split('\n');
    let hex = lines[0].trim();
    if (hex.startsWith('0x')) {
        hex = hex.slice(2);
    }
    return Buffer.from(hex, 'hex');
}

/**
 * Generate transfer Python code
 */
function generateTransferCode(lamports) {
    return `
import struct
lamports = ${lamports}
data = struct.pack('<IQ', 2, lamports)
cpi(2, [(0, 1, 1), (1, 1, 0)], data)
print('Transferred ${lamports} lamports')
`.trim();
}

async function main() {
    const args = process.argv.slice(2);

    if (args.length < 2) {
        console.log('Transfer SOL using PikaPython CPI');
        console.log('');
        console.log('Usage:');
        console.log('  node transfer.js <program_id> <recipient> [amount_sol] [options]');
        console.log('');
        console.log('Arguments:');
        console.log('  program_id   PikaPython program ID');
        console.log('  recipient    Recipient wallet address');
        console.log('  amount_sol   Amount to transfer in SOL (default: 0.001)');
        console.log('');
        console.log('Options:');
        console.log('  -k, --keypair <path>  Wallet keypair file');
        console.log('  -u, --url <url>       RPC URL (default: mainnet-beta)');
        console.log('');
        console.log('Example:');
        console.log('  node transfer.js Abc123... Bob456... 0.01 -k wallet.json');
        process.exit(1);
    }

    let programIdStr = null;
    let recipientStr = null;
    let amountSol = 0.001;
    let keypairPath = process.env.KEYPAIR_PATH || DEFAULT_KEYPAIR_PATH;
    let rpcUrl = process.env.RPC_URL || DEFAULT_RPC_URL;

    // Parse arguments
    const positional = [];
    for (let i = 0; i < args.length; i++) {
        if ((args[i] === '-k' || args[i] === '--keypair') && args[i + 1]) {
            keypairPath = args[++i];
        } else if ((args[i] === '-u' || args[i] === '--url') && args[i + 1]) {
            rpcUrl = args[++i];
        } else {
            positional.push(args[i]);
        }
    }

    programIdStr = positional[0];
    recipientStr = positional[1];
    if (positional[2]) amountSol = parseFloat(positional[2]);
    const lamports = Math.floor(amountSol * 1e9);

    // Validate inputs
    let programId, recipient;
    try {
        programId = new PublicKey(programIdStr);
    } catch {
        console.error(`Error: Invalid program ID: ${programIdStr}`);
        process.exit(1);
    }
    try {
        recipient = new PublicKey(recipientStr);
    } catch {
        console.error(`Error: Invalid recipient address: ${recipientStr}`);
        process.exit(1);
    }

    // Connect
    console.log(`Connecting to ${rpcUrl}...`);
    const connection = new Connection(rpcUrl, 'confirmed');

    // Load wallet
    if (!fs.existsSync(keypairPath)) {
        console.error(`Error: Keypair not found at ${keypairPath}`);
        process.exit(1);
    }
    const payerKeypair = JSON.parse(fs.readFileSync(keypairPath, 'utf8'));
    const payer = Keypair.fromSecretKey(new Uint8Array(payerKeypair));

    const balance = await connection.getBalance(payer.publicKey);
    console.log('');
    console.log('=== Transfer Details ===');
    console.log(`From:      ${payer.publicKey.toString()}`);
    console.log(`To:        ${recipient.toString()}`);
    console.log(`Amount:    ${amountSol} SOL (${lamports} lamports)`);
    console.log(`Balance:   ${balance / 1e9} SOL`);
    console.log(`Program:   ${programId.toString()}`);
    console.log('');

    if (balance < lamports + 10000) {
        console.error('Error: Insufficient balance for transfer + fees');
        process.exit(1);
    }

    // Compile transfer code
    console.log('Compiling transfer script...');
    const code = generateTransferCode(lamports);
    const bytecode = compile(code);
    console.log(`Bytecode: ${bytecode.length} bytes`);
    console.log('');

    // Build instruction
    const data = Buffer.concat([Buffer.from([EXECUTE_BYTECODE]), bytecode]);

    const instruction = new TransactionInstruction({
        keys: [
            { pubkey: payer.publicKey, isSigner: true, isWritable: true },   // Account 0: Payer
            { pubkey: recipient, isSigner: false, isWritable: true },         // Account 1: Recipient
            { pubkey: SystemProgram.programId, isSigner: false, isWritable: false }, // Account 2: System Program
        ],
        programId,
        data,
    });

    const transaction = new Transaction();
    transaction.add(
        ComputeBudgetProgram.setComputeUnitLimit({ units: 200_000 }),
        ComputeBudgetProgram.requestHeapFrame({ bytes: 256 * 1024 }),
        instruction
    );

    transaction.feePayer = payer.publicKey;
    transaction.recentBlockhash = (await connection.getLatestBlockhash()).blockhash;

    // Simulate
    console.log('Simulating transaction...');
    const simResult = await connection.simulateTransaction(transaction);

    if (simResult.value.err) {
        const logs = simResult.value.logs?.join('\n') || 'No logs';
        console.error(`Simulation failed: ${JSON.stringify(simResult.value.err)}`);
        console.error(`Logs:\n${logs}`);
        process.exit(1);
    }

    // Extract CU and output
    let cu = null;
    const cuLog = simResult.value.logs?.find(log => log.includes('consumed'));
    if (cuLog) {
        const match = cuLog.match(/consumed (\d+) of/);
        if (match) cu = parseInt(match[1]);
    }

    let output = null;
    if (simResult.value.returnData) {
        const rd = simResult.value.returnData;
        output = Buffer.from(rd.data[0], rd.data[1]).toString('utf8');
    }

    console.log(`Simulation successful (${cu} CU)`);
    if (output) console.log(`Output: ${output}`);
    console.log('');

    // Confirm
    const readline = require('readline');
    const rl = readline.createInterface({
        input: process.stdin,
        output: process.stdout
    });

    const answer = await new Promise(resolve => {
        rl.question(`Transfer ${amountSol} SOL to ${recipientStr.slice(0, 8)}...? (y/N) `, resolve);
    });
    rl.close();

    if (answer.toLowerCase() !== 'y') {
        console.log('Transfer cancelled.');
        process.exit(0);
    }

    // Send
    console.log('Sending transaction...');
    const signature = await sendAndConfirmTransaction(connection, transaction, [payer], {
        commitment: 'confirmed',
    });

    console.log('');
    console.log('=== Transfer Complete ===');
    console.log(`Signature: ${signature}`);
    console.log(`Explorer:  https://explorer.solana.com/tx/${signature}`);

    // Check new balances
    const newPayerBalance = await connection.getBalance(payer.publicKey);
    const recipientBalance = await connection.getBalance(recipient);
    console.log('');
    console.log('New balances:');
    console.log(`  Payer:     ${newPayerBalance / 1e9} SOL`);
    console.log(`  Recipient: ${recipientBalance / 1e9} SOL`);
}

main().catch(err => {
    console.error(`Error: ${err.message}`);
    process.exit(1);
});
