/**
 * PikaPython Emscripten Bytecode Compiler
 * Compiles Python source to PikaPython bytecode in the browser.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <emscripten.h>

// PikaPython configuration
#define PIKA_ASSERT_ENABLE 0
#define PIKA_STD_DEVICE_UNSUPPORTED 1
#define PIKA_FILEIO_ENABLE 0
#define PIKA_FLOAT_TYPE_DOUBLE 0
#define PIKA_STACK_BUFF_SIZE 512
#define PIKA_ARG_CACHE_ENABLE 0
#define PIKA_SYNTAX_SLICE_ENABLE 1
#define PIKA_SYNTAX_FORMAT_ENABLE 0
#define PIKA_SYNTAX_IMPORT_EX_ENABLE 1
#define PIKA_SYNTAX_EXCEPTION_ENABLE 1
#define PIKA_EVENT_ENABLE 0
#define PIKA_GC_MARK_SWEEP_ENABLE 0
#define PIKA_DEBUG_BREAK_POINT_MAX 0
#define PIKA_BUILTIN_STRUCT_ENABLE 0
#define PIKA_MATH_ENABLE 0

// PikaPython core includes
#include "../../src/PikaPlatform.c"
#include "../../src/dataMemory.c"
#include "../../src/dataArg.c"
#include "../../src/dataArgs.c"
#include "../../src/dataLink.c"
#include "../../src/dataLinkNode.c"
#include "../../src/dataString.c"
#include "../../src/dataStrs.c"
#include "../../src/dataQueue.c"
#include "../../src/PikaObj.c"
#include "../../src/TinyObj.c"
#include "../../src/BaseObj.c"
#include "../../port/solana_sbf/PikaStdData_List_sbf.c"
#include "../../port/solana_sbf/PikaStdData_Tuple_sbf.c"
#include "../../port/solana_sbf/PikaStdData_Dict_sbf.c"
#include "../../src/dataStack.c"
#include "../../src/dataQueueObj.c"
#include "../../src/PikaParser.c"
#include "../../src/PikaCompiler.c"
#include "../../src/PikaVM.c"

// Stub functions
volatile PikaObj* __pikaMain = NULL;
PikaObj* New_PikaStdData_ByteArray(Args* args) { return NULL; }
PikaObj* New_PikaStdData_String(Args* args) { return NULL; }
PikaObj* New_PikaStdLib_SysObj(Args* args) { return NULL; }
PikaObj* New_builtins(Args* args) { return NULL; }
PikaObj* New_builtins_RangeObj(Args* args) { return NULL; }
PikaObj* New_builtins_object(Args* args) { return NULL; }
int strGetSizeUtf8(char* str) { return (int)strlen(str); }
char* string_slice(Args* outBuffs, char* str, int start, int end) { return NULL; }

// Bytecode magic header
static const uint8_t BYTECODE_MAGIC[] = {0x0f, 'p', 'y', 'o'};

// Output buffer
static uint8_t* output_buffer = NULL;
static uint32_t output_size = 0;

EMSCRIPTEN_KEEPALIVE
uint8_t* get_output_ptr(void) {
    return output_buffer;
}

EMSCRIPTEN_KEEPALIVE
uint32_t get_output_size(void) {
    return output_size;
}

EMSCRIPTEN_KEEPALIVE
int compile_python(const char* source) {
    // Free previous output
    if (output_buffer) {
        free(output_buffer);
        output_buffer = NULL;
        output_size = 0;
    }

    // Initialize bytecode frame
    ByteCodeFrame bcf;
    memset(&bcf, 0, sizeof(bcf));
    byteCodeFrame_init(&bcf);

    // Parse Python to bytecode
    if (PIKA_RES_OK != pika_lines2Bytes(&bcf, (char*)source)) {
        return -1;
    }

    // Get sizes
    uint32_t instruct_array_size = bcf.instruct_array.size;
    uint32_t const_pool_size = bcf.const_pool.size;

    // Calculate total size
    uint32_t bytecode_body_size = 4 + instruct_array_size + 4 + const_pool_size;
    uint32_t header_size = sizeof(BYTECODE_MAGIC) + 4;
    uint32_t total_size = header_size + bytecode_body_size;

    output_buffer = (uint8_t*)malloc(total_size);
    if (!output_buffer) {
        return -2;
    }

    uint32_t pos = 0;

    // Write header: magic + body_size
    memcpy(output_buffer + pos, BYTECODE_MAGIC, sizeof(BYTECODE_MAGIC));
    pos += sizeof(BYTECODE_MAGIC);

    memcpy(output_buffer + pos, &bytecode_body_size, 4);
    pos += 4;

    // Write instruct array
    memcpy(output_buffer + pos, &instruct_array_size, 4);
    pos += 4;

    if (bcf.instruct_array.content_start && instruct_array_size > 0) {
        memcpy(output_buffer + pos, bcf.instruct_array.content_start, instruct_array_size);
        pos += instruct_array_size;
    }

    // Write const pool
    memcpy(output_buffer + pos, &const_pool_size, 4);
    pos += 4;

    if (bcf.const_pool.content_start && const_pool_size > 0) {
        memcpy(output_buffer + pos, bcf.const_pool.content_start, const_pool_size);
        pos += const_pool_size;
    }

    output_size = pos;
    return 0;
}
