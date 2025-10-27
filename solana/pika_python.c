// PikaPython Unified Program for Solana SBF
//
// Three execution modes determined by first byte:
//   0x00 = EXECUTE_SCRIPT:   Parse and execute Python source (combined)
//   0x01 = GENERATE_BYTECODE: Parse Python source, return bytecode
//   0x02 = EXECUTE_BYTECODE:  Execute pre-compiled bytecode
//
// Instruction data format:
//   [mode: u8] [payload...]
//
// For EXECUTE_SCRIPT and GENERATE_BYTECODE:
//   payload = Python source code (UTF-8)
//
// For EXECUTE_BYTECODE:
//   payload = bytecode (starts with magic 0x0f 'p' 'y' 'o')

#include <stdint.h>

// Forward declare our printf variadic function
void _pika_platform_printf_variadic(const char* fmt,
    intptr_t a1, intptr_t a2, intptr_t a3, intptr_t a4,
    intptr_t a5, intptr_t a6, intptr_t a7, intptr_t a8);

// Override pika_platform_printf to use our variadic handler
#define pika_platform_printf ((void(*)(char*, ...))_pika_platform_printf_variadic)

#include <solana_sdk.h>
#include <sol/return_data.h>

// PikaPython Includes (paths relative to solana/ directory)
#include "../port/solana_sbf/PikaPlatform_sbf.c"
#include "../port/solana_sbf/builtins_sbf.c"
#include "../src/dataMemory.c"
#include "../src/dataArg.c"
#include "../src/dataArgs.c"
#include "../src/dataLink.c"
#include "../src/dataLinkNode.c"
#include "../src/dataString.c"
#include "../src/dataStrs.c"
#include "../src/PikaObj.c"
#include "../src/TinyObj.c"
#include "../port/solana_sbf/PikaStdData_List_sbf.c"
#include "../port/solana_sbf/PikaStdData_Tuple_sbf.c"
#include "../port/solana_sbf/PikaStdData_Dict_sbf.c"
#include "../src/dataStack.c"
#include "../src/dataQueueObj.c"
#include "../src/PikaParser.c"
#include "../src/PikaCompiler.c"
#include "../src/PikaVM.c"
#include "../port/solana_sbf/solana_vfs.c"

// Math module (SBF-specific version using float functions due to buggy libm doubles)
#include "pikascript-api/_math.h"
#include "pikascript-api/_math_sbf.c"
#include "pikascript-api/__pikaBinding.c"

// Execution modes
typedef enum {
    MODE_EXECUTE_SCRIPT = 0x00,    // Parse + execute Python source
    MODE_GENERATE_BYTECODE = 0x01, // Parse Python, return bytecode
    MODE_EXECUTE_BYTECODE = 0x02,  // Execute pre-compiled bytecode
    MODE_WRITE_ACCOUNT = 0x03,     // Write raw bytes to account 0
} PikaMode;

// Bytecode magic
static const uint8_t BYTECODE_MAGIC[] = {0x0f, 'p', 'y', 'o'};

// Check if data starts with bytecode magic
static int is_valid_bytecode(const uint8_t* data, uint64_t len) {
    if (len < 4) return 0;
    return (data[0] == BYTECODE_MAGIC[0] &&
            data[1] == BYTECODE_MAGIC[1] &&
            data[2] == BYTECODE_MAGIC[2] &&
            data[3] == BYTECODE_MAGIC[3]);
}

// Helper to log bytecode as hex in single line: "prefix (N bytes): 0x..."
static void log_bytecode(const char* prefix, const uint8_t* data, uint64_t len) {
    static const char hex[] = "0123456789abcdef";
    char buf[512];
    int pos = 0;

    // Copy prefix
    for (int i = 0; prefix[i] && pos < 100; i++) {
        buf[pos++] = prefix[i];
    }

    // Add " ("
    buf[pos++] = ' ';
    buf[pos++] = '(';

    // Convert len to decimal
    char num[20];
    int npos = 0;
    uint64_t n = len;
    if (n == 0) {
        num[npos++] = '0';
    } else {
        while (n > 0) {
            num[npos++] = '0' + (n % 10);
            n /= 10;
        }
    }
    for (int i = npos - 1; i >= 0; i--) {
        buf[pos++] = num[i];
    }

    // Add " bytes): 0x"
    const char* suffix = " bytes): 0x";
    for (int i = 0; suffix[i]; i++) {
        buf[pos++] = suffix[i];
    }

    // Add hex bytes (up to ~180 bytes = 360 hex chars to fit in 512 buf)
    uint64_t to_log = len < 180 ? len : 180;
    for (uint64_t i = 0; i < to_log; i++) {
        buf[pos++] = hex[(data[i] >> 4) & 0xF];
        buf[pos++] = hex[data[i] & 0xF];
    }
    if (len > 180) {
        buf[pos++] = '.';
        buf[pos++] = '.';
        buf[pos++] = '.';
    }
    buf[pos] = '\0';
    sol_log(buf);
}

// Forward declarations
extern VMParameters* pikaVM_run_ex(PikaObj* self, char* py_lines, pikaVM_run_ex_cfg* cfg);
extern VMParameters* _pikaVM_runByteCodeFrame(PikaObj* self, ByteCodeFrame* byteCode_frame, pika_bool in_repl);

/**
 * Mode 0x00: Execute Python script (parse + run)
 * Returns: execution result via return data (REPL-style)
 */
static uint64_t execute_script(const uint8_t* data, uint64_t len) {

    // Allocate and copy source with null terminator
    char* py_code = (char*)pika_platform_malloc(len + 1);
    if (!py_code) {
        sol_log("Error: OOM allocating script buffer");
        return 1;
    }
    sol_memcpy(py_code, data, len);
    py_code[len] = '\0';

    // Parse Python to bytecode first (so we can log it)
    ByteCodeFrame bcf;
    pika_platform_memset(&bcf, 0, sizeof(bcf));
    byteCodeFrame_init(&bcf);

    if (PIKA_RES_OK != pika_lines2Bytes(&bcf, py_code)) {
        sol_log("Error: Parse failed");
        pika_platform_free(py_code);
        byteCodeFrame_deinit(&bcf);
        return 1;
    }

    pika_platform_free(py_code);

    // Build bytecode buffer for logging
    uint32_t instruct_array_size = bcf.instruct_array.size;
    uint32_t const_pool_size = bcf.const_pool.size;
    uint32_t bytecode_body_size = 4 + instruct_array_size + 4 + const_pool_size;
    uint32_t header_size = sizeof(BYTECODE_MAGIC) + 4;
    uint32_t total_size = header_size + bytecode_body_size;

    uint8_t* bytecode_buf = (uint8_t*)pika_platform_malloc(total_size);
    if (!bytecode_buf) {
        sol_log("Error: OOM allocating bytecode buffer for logging");
        byteCodeFrame_deinit(&bcf);
        return 1;
    }

    uint32_t pos = 0;
    sol_memcpy(bytecode_buf + pos, BYTECODE_MAGIC, sizeof(BYTECODE_MAGIC));
    pos += sizeof(BYTECODE_MAGIC);
    sol_memcpy(bytecode_buf + pos, &bytecode_body_size, 4);
    pos += 4;
    sol_memcpy(bytecode_buf + pos, &instruct_array_size, 4);
    pos += 4;
    if (bcf.instruct_array.content_start && instruct_array_size > 0) {
        sol_memcpy(bytecode_buf + pos, bcf.instruct_array.content_start, instruct_array_size);
        pos += instruct_array_size;
    }
    sol_memcpy(bytecode_buf + pos, &const_pool_size, 4);
    pos += 4;
    if (bcf.const_pool.content_start && const_pool_size > 0) {
        sol_memcpy(bytecode_buf + pos, bcf.const_pool.content_start, const_pool_size);
        pos += const_pool_size;
    }

    // Log the generated bytecode
    log_bytecode("Generated bytecode:", bytecode_buf, total_size);

    pika_platform_free(bytecode_buf);

    // Create root object
    PikaObj* root = New_PikaObj(NULL);
    if (!root) {
        sol_log("Error: Failed to create root object");
        byteCodeFrame_deinit(&bcf);
        return 1;
    }

    // Run bytecode in REPL mode (enables result capture via pika_hook_unused_stack_arg)
    VMParameters* result = _pikaVM_runByteCodeFrame(root, &bcf, pika_true);

    byteCodeFrame_deinit(&bcf);

    if (!result) {
        sol_log("Error: Script execution failed");
        return 1;
    }

    return 0;
}

/**
 * Mode 0x01: Generate bytecode from Python source
 * Returns: bytecode via return data
 */
static uint64_t generate_bytecode(const uint8_t* data, uint64_t len) {
    // Allocate and copy source with null terminator
    char* py_code = (char*)pika_platform_malloc(len + 1);
    if (!py_code) {
        sol_log("Error: OOM allocating source buffer");
        return 1;
    }
    sol_memcpy(py_code, data, len);
    py_code[len] = '\0';

    // Parse Python to bytecode frame
    ByteCodeFrame bcf;
    pika_platform_memset(&bcf, 0, sizeof(bcf));
    byteCodeFrame_init(&bcf);

    if (PIKA_RES_OK != pika_lines2Bytes(&bcf, py_code)) {
        sol_log("Error: Parse failed");
        pika_platform_free(py_code);
        byteCodeFrame_deinit(&bcf);
        return 1;
    }

    pika_platform_free(py_code);

    // Get sizes from compiled bytecode
    uint32_t instruct_array_size = bcf.instruct_array.size;
    uint32_t const_pool_size = bcf.const_pool.size;

    // Format: magic(4) + total_size(4) + instruct_size(4) + instruct_array + const_size(4) + const_pool
    uint32_t bytecode_body_size = 4 + instruct_array_size + 4 + const_pool_size;
    uint32_t header_size = sizeof(BYTECODE_MAGIC) + 4;
    uint32_t total_size = header_size + bytecode_body_size;

    // Allocate output buffer
    uint8_t* output = (uint8_t*)pika_platform_malloc(total_size);
    if (!output) {
        sol_log("Error: OOM allocating output buffer");
        byteCodeFrame_deinit(&bcf);
        return 1;
    }

    uint32_t pos = 0;

    // Write header: magic + body_size
    sol_memcpy(output + pos, BYTECODE_MAGIC, sizeof(BYTECODE_MAGIC));
    pos += sizeof(BYTECODE_MAGIC);

    sol_memcpy(output + pos, &bytecode_body_size, 4);
    pos += 4;

    // Write bytecode body
    sol_memcpy(output + pos, &instruct_array_size, 4);
    pos += 4;

    if (bcf.instruct_array.content_start && instruct_array_size > 0) {
        sol_memcpy(output + pos, bcf.instruct_array.content_start, instruct_array_size);
        pos += instruct_array_size;
    }

    sol_memcpy(output + pos, &const_pool_size, 4);
    pos += 4;

    if (bcf.const_pool.content_start && const_pool_size > 0) {
        sol_memcpy(output + pos, bcf.const_pool.content_start, const_pool_size);
        pos += const_pool_size;
    }

    byteCodeFrame_deinit(&bcf);

    // Set bytecode as return data
    sol_set_return_data(output, pos);
    pika_platform_free(output);

    return 0;
}

/**
 * Mode 0x02: Execute pre-compiled bytecode
 * Returns: execution result via return data
 */
static uint64_t execute_bytecode(const uint8_t* data, uint64_t len) {

    // Verify bytecode magic
    if (!is_valid_bytecode(data, len)) {
        sol_log("Error: Invalid bytecode (bad magic)");
        return 1;
    }

    // Log the bytecode being executed
    log_bytecode("Input bytecode:", data, len);

    // Copy bytecode to heap for alignment
    uint8_t* bytecode = (uint8_t*)pika_platform_malloc(len);
    if (!bytecode) {
        sol_log("Error: OOM allocating bytecode buffer");
        return 1;
    }
    sol_memcpy(bytecode, data, len);

    // Parse bytecode format: magic(4) + body_size(4) + instruct_size(4) + instruct_array + const_size(4) + const_pool
    uint32_t pos = sizeof(BYTECODE_MAGIC);
    uint32_t body_size;
    sol_memcpy(&body_size, bytecode + pos, 4);
    pos += 4;

    uint32_t instruct_size;
    sol_memcpy(&instruct_size, bytecode + pos, 4);
    pos += 4;

    uint8_t* instruct_data = bytecode + pos;
    pos += instruct_size;

    uint32_t const_size;
    sol_memcpy(&const_size, bytecode + pos, 4);
    pos += 4;

    uint8_t* const_data = bytecode + pos;

    // Build ByteCodeFrame from parsed data
    ByteCodeFrame bcf;
    pika_platform_memset(&bcf, 0, sizeof(bcf));
    byteCodeFrame_init(&bcf);

    // Set up instruct array
    bcf.instruct_array.size = instruct_size;
    bcf.instruct_array.content_start = instruct_data;

    // Set up const pool
    bcf.const_pool.size = const_size;
    bcf.const_pool.content_start = (char*)const_data;

    // Create root object
    PikaObj* root = New_PikaObj(NULL);
    if (!root) {
        sol_log("Error: Failed to create root object");
        pika_platform_free(bytecode);
        return 1;
    }

    // Run bytecode in REPL mode (enables result capture via pika_hook_unused_stack_arg)
    VMParameters* result = _pikaVM_runByteCodeFrame(root, &bcf, pika_true);

    pika_platform_free(bytecode);

    if (!result) {
        sol_log("Error: Bytecode execution failed");
        return 1;
    }

    return 0;
}

/**
 * Write raw bytes to account 0
 * Mode 0x03: payload bytes are written directly to account data
 */
static uint64_t write_account(SolParameters *params, const uint8_t* data, uint64_t len) {
    if (params->ka_num < 1) {
        sol_log("Error: No account provided");
        return 1;
    }

    SolAccountInfo* acct = &params->ka[0];
    if (!acct->is_writable) {
        sol_log("Error: Account not writable");
        return 1;
    }

    if (len > acct->data_len) {
        sol_log("Error: Data too large for account");
        return 1;
    }

    /* Copy data to account */
    sol_memcpy(acct->data, data, (int)len);
    return 0;
}

/**
 * Main instruction processor - dispatches based on mode byte
 */
uint64_t process_instruction(SolParameters *params) {
    if (params->data_len < 2) {
        sol_log("Error: Instruction too short (need mode + data)");
        return 1;
    }

    // First byte is the mode
    uint8_t mode = params->data[0];
    const uint8_t* payload = params->data + 1;
    uint64_t payload_len = params->data_len - 1;

    switch (mode) {
        case MODE_EXECUTE_SCRIPT:
            return execute_script(payload, payload_len);

        case MODE_GENERATE_BYTECODE:
            return generate_bytecode(payload, payload_len);

        case MODE_EXECUTE_BYTECODE:
            return execute_bytecode(payload, payload_len);

        case MODE_WRITE_ACCOUNT:
            return write_account(params, payload, payload_len);

        default:
            sol_log("Error: Unknown mode");
            return 1;
    }
}

/* Maximum accounts that can be passed for VFS use */
#define MAX_ACCOUNTS 10

/* Global storage for CPI access - stored in heap since SBF has no .bss */
typedef struct {
    SolAccountInfo* accounts;
    uint64_t num_accounts;
    const SolPubkey* program_id;
} PikaCpiContext;

/* CPI context stored in fixed heap location (after VFS table) */
#define CPI_CONTEXT_OFFSET 1560
#define CPI_CONTEXT_PTR ((PikaCpiContext*)(HEAP_START + CPI_CONTEXT_OFFSET))

/* Accessor functions for CPI context (called from builtins_sbf.c) */
SolAccountInfo* pika_get_cpi_accounts(void) {
    return CPI_CONTEXT_PTR->accounts;
}

uint64_t pika_get_cpi_num_accounts(void) {
    return CPI_CONTEXT_PTR->num_accounts;
}

const SolPubkey* pika_get_program_id(void) {
    return CPI_CONTEXT_PTR->program_id;
}

/* Store accounts for CPI access */
static void init_cpi_context(SolAccountInfo* accounts, uint64_t num_accounts, const SolPubkey* program_id) {
    PikaCpiContext* ctx = CPI_CONTEXT_PTR;
    ctx->accounts = accounts;
    ctx->num_accounts = num_accounts;
    ctx->program_id = program_id;
}

/* Convert SolAccountInfo array to SolVfsAccountInfo array for VFS init */
static void init_vfs_from_accounts(SolAccountInfo* accounts, uint64_t num_accounts) {
    uint64_t count = num_accounts < MAX_ACCOUNTS ? num_accounts : MAX_ACCOUNTS;

    /* Allocate on heap since Solana doesn't allow .bss section */
    SolVfsAccountInfo* vfs_accounts = (SolVfsAccountInfo*)pika_platform_malloc(
        count * sizeof(SolVfsAccountInfo));
    if (!vfs_accounts) {
        sol_log("Error: Failed to allocate VFS accounts");
        return;
    }

    for (uint64_t i = 0; i < count; i++) {
        vfs_accounts[i].pubkey = (uint8_t*)accounts[i].key;
        vfs_accounts[i].data = accounts[i].data;
        vfs_accounts[i].data_len = accounts[i].data_len;
        vfs_accounts[i].is_writable = accounts[i].is_writable;
        vfs_accounts[i].is_signer = accounts[i].is_signer;
    }

    sol_vfs_init(vfs_accounts, count);
}

extern uint64_t entrypoint(const uint8_t *input) {
    SolAccountInfo accounts[MAX_ACCOUNTS];
    SolParameters params = (SolParameters){.ka = accounts};
    if (!sol_deserialize(input, &params, SOL_ARRAY_SIZE(accounts))) {
        return 1;
    }

    /* Initialize VFS with transaction accounts */
    init_vfs_from_accounts(params.ka, params.ka_num);

    /* Initialize CPI context for cross-program invocations */
    init_cpi_context(params.ka, params.ka_num, params.program_id);

    return process_instruction(&params);
}
