#!/usr/bin/env node
/**
 * PikaPython Off-Chain Bytecode Demo
 *
 * This script demonstrates the off-chain bytecode workflow:
 * 1. Compile Python source to bytecode using pika_compile
 * 2. Show the bytecode format
 * 3. Demonstrate how to send pre-compiled bytecode on-chain (EXECUTE_BYTECODE mode)
 */

const fs = require('fs');
const path = require('path');
const { execSync } = require('child_process');

// Colors
const GREEN = '\x1b[32m';
const CYAN = '\x1b[36m';
const YELLOW = '\x1b[33m';
const RESET = '\x1b[0m';

// Bytecode magic header
const BYTECODE_MAGIC = Buffer.from([0x0f, 0x70, 0x79, 0x6f]); // 0x0f 'p' 'y' 'o'

function parseBytecode(buffer) {
    if (buffer.length < 8) {
        throw new Error('Bytecode too short');
    }

    // Check magic
    const magic = buffer.slice(0, 4);
    if (!magic.equals(BYTECODE_MAGIC)) {
        throw new Error('Invalid bytecode magic');
    }

    // Read body size
    const bodySize = buffer.readUInt32LE(4);

    // Read instruction array size
    const instructSize = buffer.readUInt32LE(8);

    // Read instruction array
    const instructArray = buffer.slice(12, 12 + instructSize);

    // Read const pool size
    const constPoolOffset = 12 + instructSize;
    const constPoolSize = buffer.readUInt32LE(constPoolOffset);

    // Read const pool
    const constPool = buffer.slice(constPoolOffset + 4, constPoolOffset + 4 + constPoolSize);

    return {
        magic: magic.toString('hex'),
        bodySize,
        instructSize,
        instructArray,
        constPoolSize,
        constPool,
        totalSize: buffer.length
    };
}

function showConstPool(constPool) {
    // Const pool is null-terminated strings
    const strings = [];
    let start = 0;
    for (let i = 0; i < constPool.length; i++) {
        if (constPool[i] === 0) {
            if (i > start) {
                strings.push(constPool.slice(start, i).toString('utf8'));
            }
            start = i + 1;
        }
    }
    return strings;
}

function compileExample(name) {
    const srcPath = path.join(__dirname, `${name}.py`);
    const binPath = path.join(__dirname, 'bytecode', `${name}.bin`);

    console.log(`\n${CYAN}=== ${name}.py ===${RESET}`);

    // Show source
    const source = fs.readFileSync(srcPath, 'utf8');
    console.log(`${YELLOW}Source:${RESET}`);
    source.trim().split('\n').forEach(line => console.log(`  ${line}`));

    // Load bytecode
    const bytecode = fs.readFileSync(binPath);
    const parsed = parseBytecode(bytecode);

    console.log(`\n${YELLOW}Bytecode:${RESET}`);
    console.log(`  Magic: ${parsed.magic} (${BYTECODE_MAGIC.toString()})`);
    console.log(`  Total size: ${parsed.totalSize} bytes`);
    console.log(`  Instruction array: ${parsed.instructSize} bytes`);
    console.log(`  Const pool: ${parsed.constPoolSize} bytes`);

    // Show const pool contents
    const constants = showConstPool(parsed.constPool);
    if (constants.length > 0) {
        console.log(`  Constants: ${JSON.stringify(constants)}`);
    }

    // Show hex for on-chain use
    console.log(`\n${YELLOW}Hex (for on-chain EXECUTE_BYTECODE mode):${RESET}`);
    const hex = bytecode.toString('hex');
    if (hex.length > 80) {
        console.log(`  ${hex.slice(0, 80)}...`);
    } else {
        console.log(`  ${hex}`);
    }

    return { name, source, bytecode, parsed };
}

function showOnChainUsage(examples) {
    console.log(`\n${CYAN}========================================${RESET}`);
    console.log(`${CYAN}On-Chain Usage (EXECUTE_BYTECODE mode)${RESET}`);
    console.log(`${CYAN}========================================${RESET}`);

    console.log(`
To execute pre-compiled bytecode on-chain:

1. Use mode byte 0x02 (EXECUTE_BYTECODE) instead of 0x00 (EXECUTE_SCRIPT)
2. Append the raw bytecode (including magic header)

${YELLOW}JavaScript example:${RESET}

const bytecode = fs.readFileSync('examples/bytecode/hello.bin');
const data = Buffer.concat([
    Buffer.from([0x02]),  // EXECUTE_BYTECODE mode
    bytecode
]);

const instruction = new TransactionInstruction({
    keys: [{ pubkey: payer.publicKey, isSigner: true, isWritable: true }],
    programId,
    data,
});

${YELLOW}Benefits of pre-compiled bytecode:${RESET}
- Saves compute units (no parsing/compilation on-chain)
- Faster execution
- Allows more complex scripts within CU limits
- Consistent execution (same bytecode = same behavior)

${YELLOW}Execution modes:${RESET}
- 0x00: EXECUTE_SCRIPT  - Parse Python, compile, execute
- 0x01: GENERATE_BYTECODE - Parse Python, compile, return bytecode
- 0x02: EXECUTE_BYTECODE - Execute pre-compiled bytecode directly
`);
}

async function main() {
    console.log(`${CYAN}========================================${RESET}`);
    console.log(`${CYAN}PikaPython Off-Chain Bytecode Demo${RESET}`);
    console.log(`${CYAN}========================================${RESET}`);

    // Check bytecode directory exists
    const bytecodeDir = path.join(__dirname, 'bytecode');
    if (!fs.existsSync(bytecodeDir)) {
        console.error('Bytecode directory not found. Run compile_examples.sh first.');
        process.exit(1);
    }

    // Process each example
    const examples = ['hello', 'math', 'loop', 'function', 'list_ops'];
    const compiled = [];

    for (const name of examples) {
        try {
            compiled.push(compileExample(name));
        } catch (err) {
            console.error(`Error processing ${name}: ${err.message}`);
        }
    }

    // Show on-chain usage
    showOnChainUsage(compiled);

    console.log(`${GREEN}Done! ${compiled.length} examples compiled.${RESET}`);
}

main().catch(console.error);
