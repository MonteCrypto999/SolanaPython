/**
 * PikaPython Bytecode Compiler
 *
 * Compiles Python source to PikaPython bytecode.
 * Works both as native CLI and as WASM module for browser.
 *
 * Native usage:
 *   ./pika_compile "print('Hello')"           Output hex to stdout
 *   ./pika_compile -o output.bin "print(1)"   Write binary to file
 *   ./pika_compile -f script.py               Compile from file
 *
 * WASM usage:
 *   compile_python(source) -> returns 0 on success
 *   get_output_ptr() -> pointer to bytecode
 *   get_output_size() -> size of bytecode
 */

// Standard includes FIRST (before PikaPython which may override)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#define EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define EXPORT
#endif

// Configuration - these should match what's set in build_compiler.sh
// (defined via -D flags in compiler command line)
#define PIKA_BUILTIN_STRUCT_ENABLE 1

// PikaPython core includes (paths relative to solana/tools/ directory)
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

// Stub functions for symbols we don't need
volatile PikaObj* __pikaMain = NULL;
PikaObj* New_PikaStdData_ByteArray(Args* args) { return NULL; }
PikaObj* New_PikaStdData_String(Args* args) { return NULL; }
PikaObj* New_PikaStdLib_SysObj(Args* args) { return NULL; }
PikaObj* New_builtins(Args* args) { return NULL; }
PikaObj* New_builtins_RangeObj(Args* args) { return NULL; }
PikaObj* New_builtins_object(Args* args) { return NULL; }
int strGetSizeUtf8(char* str) { return (int)strlen(str); }
// Stub for string slicing (runtime only, not needed for compilation)
char* string_slice(Args* outBuffs, char* str, int start, int end) { return NULL; }

// Bytecode magic header
static const uint8_t BYTECODE_MAGIC[] = {0x0f, 'p', 'y', 'o'};

// Output buffer for WASM
static uint8_t* output_buffer = NULL;
static uint32_t output_size = 0;

EXPORT
uint8_t* get_output_ptr(void) {
    return output_buffer;
}

EXPORT
uint32_t get_output_size(void) {
    return output_size;
}

static int compile_to_bytecode(const char* source, uint8_t** out_bytecode, uint32_t* out_size) {
    // Initialize bytecode frame
    ByteCodeFrame bcf;
    memset(&bcf, 0, sizeof(bcf));
    byteCodeFrame_init(&bcf);

    // Parse Python to bytecode
    if (PIKA_RES_OK != pika_lines2Bytes(&bcf, (char*)source)) {
#ifndef __EMSCRIPTEN__
        fprintf(stderr, "Error: Failed to parse Python source\n");
#endif
        return -1;
    }

    // Get sizes
    uint32_t instruct_array_size = bcf.instruct_array.size;
    uint32_t const_pool_size = bcf.const_pool.size;

    // Format: magic(4) + body_size(4) + instruct_size(4) + instruct_array + const_size(4) + const_pool
    uint32_t bytecode_body_size = 4 + instruct_array_size + 4 + const_pool_size;
    uint32_t header_size = sizeof(BYTECODE_MAGIC) + 4;
    uint32_t total_size = header_size + bytecode_body_size;

    uint8_t* output = malloc(total_size);
    if (!output) {
#ifndef __EMSCRIPTEN__
        fprintf(stderr, "Error: Out of memory\n");
#endif
        return -1;
    }

    uint32_t pos = 0;

    // Write header: magic + body_size
    memcpy(output + pos, BYTECODE_MAGIC, sizeof(BYTECODE_MAGIC));
    pos += sizeof(BYTECODE_MAGIC);

    memcpy(output + pos, &bytecode_body_size, 4);
    pos += 4;

    // Write instruct array
    memcpy(output + pos, &instruct_array_size, 4);
    pos += 4;

    if (bcf.instruct_array.content_start && instruct_array_size > 0) {
        memcpy(output + pos, bcf.instruct_array.content_start, instruct_array_size);
        pos += instruct_array_size;
    }

    // Write const pool
    memcpy(output + pos, &const_pool_size, 4);
    pos += 4;

    if (bcf.const_pool.content_start && const_pool_size > 0) {
        memcpy(output + pos, bcf.const_pool.content_start, const_pool_size);
        pos += const_pool_size;
    }

    // Note: We skip byteCodeFrame_deinit to avoid double-free issues
    // Memory will be cleaned up when the process exits

    *out_bytecode = output;
    *out_size = pos;
    return 0;
}

// WASM entry point
EXPORT
int compile_python(const char* source) {
    // Free previous output
    if (output_buffer) {
        free(output_buffer);
        output_buffer = NULL;
        output_size = 0;
    }

    int result = compile_to_bytecode(source, &output_buffer, &output_size);
    return result;
}

#ifndef __EMSCRIPTEN__
// Native CLI code

static void print_usage(const char* prog) {
    fprintf(stderr, "PikaPython Native Bytecode Compiler\n\n");
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  %s \"print('Hello')\"           Compile and output hex\n", prog);
    fprintf(stderr, "  %s -o output.bin \"code\"       Write binary to file\n", prog);
    fprintf(stderr, "  %s -f script.py               Compile from file\n", prog);
    fprintf(stderr, "  %s -f script.py -o out.bin    Compile file to binary\n", prog);
    fprintf(stderr, "\nOptions:\n");
    fprintf(stderr, "  -o <file>   Write binary bytecode to file\n");
    fprintf(stderr, "  -f <file>   Read Python source from file\n");
    fprintf(stderr, "  -h, --help  Show this help\n");
}

static char* read_file(const char* filename) {
    FILE* f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "Error: Cannot open file '%s'\n", filename);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* content = malloc(size + 1);
    if (!content) {
        fclose(f);
        fprintf(stderr, "Error: Out of memory\n");
        return NULL;
    }

    size_t read_size = fread(content, 1, size, f);
    content[read_size] = '\0';
    fclose(f);

    return content;
}

int main(int argc, char** argv) {
    const char* output_file = NULL;
    const char* input_file = NULL;
    const char* source = NULL;

    // Parse arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: -o requires a filename\n");
                return 1;
            }
            output_file = argv[++i];
        } else if (strcmp(argv[i], "-f") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: -f requires a filename\n");
                return 1;
            }
            input_file = argv[++i];
        } else if (argv[i][0] != '-') {
            source = argv[i];
        } else {
            fprintf(stderr, "Error: Unknown option '%s'\n", argv[i]);
            return 1;
        }
    }

    // Get source code
    char* file_content = NULL;
    if (input_file) {
        file_content = read_file(input_file);
        if (!file_content) return 1;
        source = file_content;
    }

    if (!source) {
        fprintf(stderr, "Error: No source code provided\n");
        print_usage(argv[0]);
        return 1;
    }

    // Compile
    uint8_t* bytecode = NULL;
    uint32_t bytecode_size = 0;

    if (compile_to_bytecode(source, &bytecode, &bytecode_size) != 0) {
        if (file_content) free(file_content);
        return 1;
    }

    // Output
    if (output_file) {
        // Write binary to file
        FILE* f = fopen(output_file, "wb");
        if (!f) {
            fprintf(stderr, "Error: Cannot open output file '%s'\n", output_file);
            free(bytecode);
            if (file_content) free(file_content);
            return 1;
        }
        fwrite(bytecode, 1, bytecode_size, f);
        fclose(f);
        fprintf(stderr, "Wrote %u bytes to %s\n", bytecode_size, output_file);
    } else {
        // Output hex to stdout
        printf("0x");
        for (uint32_t i = 0; i < bytecode_size; i++) {
            printf("%02x", bytecode[i]);
        }
        printf("\n");
        fprintf(stderr, "Bytecode size: %u bytes\n", bytecode_size);
    }

    free(bytecode);
    if (file_content) free(file_content);
    return 0;
}
#endif
