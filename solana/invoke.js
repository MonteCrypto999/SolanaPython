#!/usr/bin/env node
/**
 * PikaPython Unified Invocation Script
 *
 * Invokes the unified PikaPython program with three modes:
 *   0x00 = EXECUTE_SCRIPT    - Parse and execute Python source
 *   0x01 = GENERATE_BYTECODE - Parse Python, return bytecode
 *   0x02 = EXECUTE_BYTECODE  - Execute pre-compiled bytecode
 *
 * Usage:
 *   node invoke.js "print('Hello')"                    Execute Python (mode 0x00)
 *   node invoke.js --compile "print('Hello')"          Compile only (mode 0x01)
 *   node invoke.js --bytecode 0x0f70796f...            Execute bytecode (mode 0x02)
 *   node invoke.js --chain "print('Hello')"            Compile + execute in one tx
 */

const {
    Connection,
    Keypair,
    PublicKey,
    Transaction,
    TransactionInstruction,
    sendAndConfirmTransaction,
    ComputeBudgetProgram,
} = require('@solana/web3.js');
const fs = require('fs');
const path = require('path');

// Configuration
const RPC_URL = 'http://localhost:8899';
const PIKA_KEYPAIR_PATH = path.join(__dirname, 'pika-keypair.json');

// Execution modes
const MODE_EXECUTE_SCRIPT = 0x00;
const MODE_GENERATE_BYTECODE = 0x01;
const MODE_EXECUTE_BYTECODE = 0x02;

// Colors for output
const GREEN = '\x1b[32m';
const RED = '\x1b[31m';
const YELLOW = '\x1b[33m';
const CYAN = '\x1b[36m';
const RESET = '\x1b[0m';

/**
 * Load program ID from keypair file
 */
function loadProgramId(keypairPath) {
    const keypairData = JSON.parse(fs.readFileSync(keypairPath, 'utf8'));
    const keypair = Keypair.fromSecretKey(new Uint8Array(keypairData));
    return keypair.publicKey;
}

/**
 * Load payer keypair
 */
function loadPayer() {
    const payerKeypairPath = process.env.HOME + '/.config/solana/id.json';
    const payerKeypair = JSON.parse(fs.readFileSync(payerKeypairPath, 'utf8'));
    return Keypair.fromSecretKey(new Uint8Array(payerKeypair));
}

/**
 * Build instruction data with mode prefix
 */
function buildInstructionData(mode, payload) {
    const modeBuffer = Buffer.alloc(1);
    modeBuffer.writeUInt8(mode, 0);
    return Buffer.concat([modeBuffer, payload]);
}

/**
 * Execute a single instruction
 */
async function executeInstruction(connection, programId, payer, mode, payload, description) {
    console.log(`${CYAN}[${description}]${RESET}`);

    const data = buildInstructionData(mode, payload);

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

    // Simulate first
    const simResult = await connection.simulateTransaction(transaction);

    if (simResult.value.err) {
        console.error(`${RED}Execution failed:${RESET}`, JSON.stringify(simResult.value.err));
        console.log('\nLogs:');
        simResult.value.logs?.forEach(log => console.log(`  ${log}`));
        throw new Error('Execution failed');
    }

    // Extract CU usage
    let cu = null;
    const cuLog = simResult.value.logs?.find(log => log.includes('consumed'));
    if (cuLog) {
        const match = cuLog.match(/consumed (\d+) of/);
        if (match) cu = match[1];
    }
    console.log(`  ${GREEN}Compute units: ${cu || 'N/A'}${RESET}`);

    // Always show logs in verbose mode
    if (process.env.VERBOSE === '1') {
        console.log('\nLogs:');
        simResult.value.logs?.forEach(log => console.log(`  ${log}`));
    }

    // Extract return data
    let returnData = null;
    if (simResult.value.returnData) {
        const rd = simResult.value.returnData;
        returnData = Buffer.from(rd.data[0], rd.data[1]);
    }

    // Send transaction
    const signature = await sendAndConfirmTransaction(connection, transaction, [payer], {
        commitment: 'confirmed',
        skipPreflight: true,
    });

    return { returnData, signature, cu };
}

/**
 * Chain mode: compile then execute in single transaction
 */
async function executeChain(connection, programId, payer, pythonSource) {
    console.log(`${CYAN}[CHAIN] Compile + Execute in one transaction${RESET}`);
    console.log(`  Source: ${pythonSource.substring(0, 50)}${pythonSource.length > 50 ? '...' : ''}`);

    const sourceBuffer = Buffer.from(pythonSource, 'utf8');

    // First simulate compile to get bytecode
    const compileData = buildInstructionData(MODE_GENERATE_BYTECODE, sourceBuffer);
    const compileIx = new TransactionInstruction({
        keys: [],
        programId,
        data: compileData,
    });

    const simTx = new Transaction();
    simTx.add(
        ComputeBudgetProgram.setComputeUnitLimit({ units: 1_400_000 }),
        ComputeBudgetProgram.requestHeapFrame({ bytes: 256 * 1024 }),
        compileIx
    );
    simTx.feePayer = payer.publicKey;
    simTx.recentBlockhash = (await connection.getLatestBlockhash()).blockhash;

    const compileSim = await connection.simulateTransaction(simTx);

    if (compileSim.value.err) {
        console.error(`${RED}Compilation failed:${RESET}`, compileSim.value.err);
        compileSim.value.logs?.forEach(log => console.log(`  ${log}`));
        throw new Error('Compilation failed');
    }

    // Extract bytecode
    if (!compileSim.value.returnData) {
        throw new Error('No bytecode returned');
    }
    const bytecode = Buffer.from(compileSim.value.returnData.data[0], compileSim.value.returnData.data[1]);

    // Extract compile CU
    let compileCU = null;
    const compileCuLog = compileSim.value.logs?.find(log => log.includes('consumed'));
    if (compileCuLog) {
        const match = compileCuLog.match(/consumed (\d+) of/);
        if (match) compileCU = match[1];
    }

    console.log(`  ${GREEN}Bytecode: ${bytecode.length} bytes${RESET}`);
    console.log(`  ${GREEN}Compile CU: ${compileCU || 'N/A'}${RESET}`);

    // Build combined transaction: compile + execute
    const executeData = buildInstructionData(MODE_EXECUTE_BYTECODE, bytecode);
    const executeIx = new TransactionInstruction({
        keys: [],
        programId,
        data: executeData,
    });

    const transaction = new Transaction();
    transaction.add(
        ComputeBudgetProgram.setComputeUnitLimit({ units: 1_400_000 }),
        ComputeBudgetProgram.requestHeapFrame({ bytes: 256 * 1024 }),
        compileIx,
        executeIx
    );

    transaction.feePayer = payer.publicKey;
    transaction.recentBlockhash = (await connection.getLatestBlockhash()).blockhash;

    // Simulate combined
    const simResult = await connection.simulateTransaction(transaction);

    if (simResult.value.err) {
        console.error(`${RED}Execution failed:${RESET}`, JSON.stringify(simResult.value.err));
        console.log('\nLogs:');
        simResult.value.logs?.forEach(log => console.log(`  ${log}`));
        throw new Error('Execution failed');
    }

    // Extract execute CU
    let executeCU = null;
    const cuLogs = simResult.value.logs?.filter(log => log.includes('consumed')) || [];
    if (cuLogs.length >= 2) {
        const match = cuLogs[1].match(/consumed (\d+) of/);
        if (match) executeCU = match[1];
    }

    console.log(`  ${GREEN}Execute CU: ${executeCU || 'N/A'}${RESET}`);

    // Extract result
    let result = null;
    if (simResult.value.returnData) {
        result = Buffer.from(simResult.value.returnData.data[0], simResult.value.returnData.data[1]).toString('utf8');
    }

    // Send transaction
    const signature = await sendAndConfirmTransaction(connection, transaction, [payer], {
        commitment: 'confirmed',
        skipPreflight: true,
    });

    return { result, signature, compileCU, executeCU };
}

/**
 * Main
 */
async function main() {
    const args = process.argv.slice(2);

    if (args.length === 0 || args.includes('--help') || args.includes('-h')) {
        console.log(`
${CYAN}PikaPython Unified Invocation${RESET}

Usage:
  node invoke.js "print('Hello')"                Execute Python script (mode 0x00)
  node invoke.js --compile "print('Hello')"      Compile only, output bytecode (mode 0x01)
  node invoke.js --bytecode 0x0f70796f...        Execute bytecode (mode 0x02)
  node invoke.js --chain "print('Hello')"        Compile + execute in one transaction

Options:
  --compile      Compile Python to bytecode (returns hex)
  --bytecode     Execute pre-compiled bytecode
  --chain        Compile and execute in single transaction
  --help, -h     Show this help
`);
        process.exit(0);
    }

    const isCompile = args.includes('--compile');
    const isBytecode = args.includes('--bytecode');
    const isChain = args.includes('--chain');

    const inputArgs = args.filter(a => !a.startsWith('--'));
    const input = inputArgs[0];

    if (!input) {
        console.error(`${RED}Error: No input provided${RESET}`);
        process.exit(1);
    }

    if (!fs.existsSync(PIKA_KEYPAIR_PATH)) {
        console.error(`${RED}Error: pika-keypair.json not found. Run ./build.sh and ./deploy.sh first.${RESET}`);
        process.exit(1);
    }

    const connection = new Connection(RPC_URL, 'confirmed');
    const payer = loadPayer();
    const programId = loadProgramId(PIKA_KEYPAIR_PATH);

    console.log(`${CYAN}================================${RESET}`);
    console.log(`${CYAN}PikaPython Invocation${RESET}`);
    console.log(`${CYAN}================================${RESET}`);
    console.log(`  Program: ${programId.toString()}\n`);

    if (isCompile) {
        // Mode 0x01: Generate bytecode
        const payload = Buffer.from(input, 'utf8');
        const { returnData, signature, cu } = await executeInstruction(
            connection, programId, payer,
            MODE_GENERATE_BYTECODE, payload,
            'GENERATE_BYTECODE (0x01)'
        );

        console.log(`\n${CYAN}Bytecode (hex):${RESET}`);
        console.log(`0x${returnData.toString('hex')}`);
        console.log(`\nSignature: ${signature}`);

    } else if (isBytecode) {
        // Mode 0x02: Execute bytecode
        if (!input.startsWith('0x')) {
            console.error(`${RED}Error: --bytecode requires hex starting with 0x${RESET}`);
            process.exit(1);
        }
        const payload = Buffer.from(input.slice(2), 'hex');
        const { returnData, signature, cu } = await executeInstruction(
            connection, programId, payer,
            MODE_EXECUTE_BYTECODE, payload,
            'EXECUTE_BYTECODE (0x02)'
        );

        if (returnData) {
            console.log(`\n${YELLOW}>>> Result: ${returnData.toString('utf8')}${RESET}`);
        }
        console.log(`\nSignature: ${signature}`);

    } else if (isChain) {
        // Chain: compile + execute in one tx
        const { result, signature, compileCU, executeCU } = await executeChain(
            connection, programId, payer, input
        );

        console.log(`\n${CYAN}================================${RESET}`);
        console.log(`${GREEN}Execution complete!${RESET}`);
        console.log(`${CYAN}================================${RESET}`);
        if (result) {
            console.log(`\n${YELLOW}>>> Result: ${result}${RESET}`);
        }
        console.log(`\nSignature: ${signature}`);

    } else {
        // Default mode 0x00: Execute script
        const payload = Buffer.from(input, 'utf8');
        const { returnData, signature, cu } = await executeInstruction(
            connection, programId, payer,
            MODE_EXECUTE_SCRIPT, payload,
            'EXECUTE_SCRIPT (0x00)'
        );

        console.log(`\n${CYAN}================================${RESET}`);
        console.log(`${GREEN}Execution complete!${RESET}`);
        console.log(`${CYAN}================================${RESET}`);
        if (returnData) {
            console.log(`\n${YELLOW}>>> Result: ${returnData.toString('utf8')}${RESET}`);
        }
        console.log(`\nSignature: ${signature}`);
    }
}

main().catch(err => {
    console.error(`${RED}Error: ${err.message}${RESET}`);
    process.exit(1);
});
