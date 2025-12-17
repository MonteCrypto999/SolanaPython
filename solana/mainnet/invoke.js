/**
 * Invoke PikaPython on Solana Mainnet
 *
 * Usage:
 *   node invoke.js <program_id> "print('hello')"     # Run Python code
 *   node invoke.js <program_id> -f hello.py          # Run Python file
 *   node invoke.js <program_id> -b bytecode.bin      # Run precompiled bytecode
 *
 * Examples:
 *   node invoke.js Abc123... "print('Hello World')"
 *   node invoke.js Abc123... "1+2+3"
 *   node invoke.js Abc123... -f ../examples/hello.py
 */

const {
    Connection,
    Keypair,
    PublicKey,
    Transaction,
    TransactionInstruction,
    ComputeBudgetProgram,
    sendAndConfirmTransaction,
} = require('@solana/web3.js');
const fs = require('fs');
const path = require('path');
const { execSync } = require('child_process');

// Configuration - defaults to mainnet
const DEFAULT_RPC_URL = 'https://api.mainnet-beta.solana.com';
const DEFAULT_KEYPAIR_PATH = `${process.env.HOME}/.config/solana/id.json`;
const COMPILER_PATH = path.join(__dirname, '../tools/pika_compile');

// Instruction types
const EXECUTE_SCRIPT = 0x00;   // Parse Python on-chain (slower, more CU)
const EXECUTE_BYTECODE = 0x02; // Pre-compiled bytecode (faster, less CU)

/**
 * Compile Python to bytecode
 */
function compile(code) {
    try {
        const result = execSync(`${COMPILER_PATH} "${code.replace(/"/g, '\\"')}"`, {
            encoding: 'utf8',
            maxBuffer: 10 * 1024 * 1024,
        });
        // Compiler outputs: "0x<hex>\nBytecode size: N bytes"
        // Extract just the hex part, removing 0x prefix
        const lines = result.trim().split('\n');
        let hex = lines[0].trim();
        if (hex.startsWith('0x')) {
            hex = hex.slice(2);
        }
        return Buffer.from(hex, 'hex');
    } catch (err) {
        throw new Error(`Compilation failed: ${err.message}`);
    }
}

/**
 * Execute code on-chain
 */
async function executeOnChain(connection, programId, payer, payload, mode, priorityFee = 0) {
    // Instruction: [mode, payload...]
    const data = Buffer.concat([Buffer.from([mode]), payload]);

    const instruction = new TransactionInstruction({
        keys: [],
        programId,
        data,
    });

    const transaction = new Transaction();

    // Add compute budget
    transaction.add(
        ComputeBudgetProgram.setComputeUnitLimit({ units: 1_400_000 }),
        ComputeBudgetProgram.requestHeapFrame({ bytes: 256 * 1024 }),
    );

    // Add priority fee if specified
    if (priorityFee > 0) {
        transaction.add(
            ComputeBudgetProgram.setComputeUnitPrice({ microLamports: priorityFee })
        );
    }

    transaction.add(instruction);

    transaction.feePayer = payer.publicKey;
    transaction.recentBlockhash = (await connection.getLatestBlockhash()).blockhash;

    // Simulate first
    console.log('Simulating transaction...');
    const simResult = await connection.simulateTransaction(transaction);

    if (simResult.value.err) {
        const logs = simResult.value.logs?.join('\n') || 'No logs';
        throw new Error(`Simulation failed: ${JSON.stringify(simResult.value.err)}\nLogs:\n${logs}`);
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

    console.log(`Simulation successful (${cu} CU)`);
    console.log(`Output: ${output || '(none)'}`);
    console.log('');

    // Confirm before sending
    const readline = require('readline');
    const rl = readline.createInterface({
        input: process.stdin,
        output: process.stdout
    });

    const answer = await new Promise(resolve => {
        rl.question('Send transaction to mainnet? (y/N) ', resolve);
    });
    rl.close();

    if (answer.toLowerCase() !== 'y') {
        console.log('Transaction cancelled.');
        return { output, cu, sent: false };
    }

    // Send transaction
    console.log('Sending transaction...');
    const signature = await sendAndConfirmTransaction(connection, transaction, [payer], {
        commitment: 'confirmed',
    });

    console.log(`Transaction confirmed: ${signature}`);
    console.log(`Explorer: https://explorer.solana.com/tx/${signature}`);

    return { output, cu, sent: true, signature };
}

/**
 * Main
 */
async function main() {
    const args = process.argv.slice(2);

    if (args.length < 2) {
        console.log('Usage:');
        console.log('  node invoke.js <program_id> "print(\'hello\')"');
        console.log('  node invoke.js <program_id> -f script.py');
        console.log('  node invoke.js <program_id> -b bytecode.bin');
        console.log('');
        console.log('Options:');
        console.log('  -k, --keypair <path>        Wallet keypair file');
        console.log('  -u, --url <url>             RPC URL (default: mainnet-beta)');
        console.log('  --script                    Send raw Python (parse on-chain, more CU)');
        console.log('  --priority <microlamports>  Set priority fee per CU');
        console.log('  --simulate                  Simulate only, don\'t send');
        console.log('');
        console.log('Environment:');
        console.log('  RPC_URL       Custom RPC endpoint');
        console.log('  KEYPAIR_PATH  Path to wallet keypair');
        process.exit(1);
    }

    const programIdStr = args[0];
    let code = null;
    let bytecode = null;
    let priorityFee = 0;
    let simulateOnly = false;
    let scriptMode = false;  // Use on-chain parser instead of bytecode
    let keypairPath = process.env.KEYPAIR_PATH || DEFAULT_KEYPAIR_PATH;
    let rpcUrl = process.env.RPC_URL || DEFAULT_RPC_URL;

    // Parse arguments
    for (let i = 1; i < args.length; i++) {
        if (args[i] === '-f' && args[i + 1]) {
            const filePath = args[++i];
            code = fs.readFileSync(filePath, 'utf8');
        } else if (args[i] === '-b' && args[i + 1]) {
            const binPath = args[++i];
            bytecode = fs.readFileSync(binPath);
        } else if ((args[i] === '-k' || args[i] === '--keypair') && args[i + 1]) {
            keypairPath = args[++i];
        } else if ((args[i] === '-u' || args[i] === '--url') && args[i + 1]) {
            rpcUrl = args[++i];
        } else if (args[i] === '--priority' && args[i + 1]) {
            priorityFee = parseInt(args[++i]);
        } else if (args[i] === '--simulate') {
            simulateOnly = true;
        } else if (args[i] === '--script') {
            scriptMode = true;
        } else if (!code && !bytecode) {
            code = args[i];
        }
    }

    if (!code && !bytecode) {
        console.error('Error: No code or bytecode specified');
        process.exit(1);
    }

    // Determine mode and payload
    let mode;
    let payload;

    if (bytecode) {
        // Pre-compiled bytecode provided
        mode = EXECUTE_BYTECODE;
        payload = bytecode;
        console.log(`Using pre-compiled bytecode: ${payload.length} bytes`);
    } else if (scriptMode) {
        // Script mode - send raw Python to be parsed on-chain
        mode = EXECUTE_SCRIPT;
        payload = Buffer.from(code, 'utf8');
        console.log(`Script mode (on-chain parser): ${payload.length} bytes`);
    } else {
        // Bytecode mode - compile locally
        if (!fs.existsSync(COMPILER_PATH)) {
            console.error(`Error: Compiler not found at ${COMPILER_PATH}`);
            console.error('Run: make compiler');
            process.exit(1);
        }
        mode = EXECUTE_BYTECODE;
        console.log('Compiling Python code...');
        payload = compile(code);
        console.log(`Bytecode: ${payload.length} bytes`);
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
    console.log(`Wallet: ${payer.publicKey.toString()}`);
    console.log(`Balance: ${balance / 1e9} SOL`);
    console.log('');

    // Parse program ID
    let programId;
    try {
        programId = new PublicKey(programIdStr);
    } catch {
        console.error(`Error: Invalid program ID: ${programIdStr}`);
        process.exit(1);
    }

    console.log(`Program: ${programId.toString()}`);
    console.log(`Mode: ${mode === EXECUTE_SCRIPT ? 'SCRIPT (0x00)' : 'BYTECODE (0x02)'}`);
    console.log(`Priority fee: ${priorityFee} microlamports/CU`);
    console.log('');

    // Execute
    const result = await executeOnChain(connection, programId, payer, payload, mode, priorityFee);

    if (result.output) {
        console.log('');
        console.log('=== Output ===');
        console.log(result.output);
    }
}

main().catch(err => {
    console.error(`Error: ${err.message}`);
    process.exit(1);
});
