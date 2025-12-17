/**
 * PikaPython WASM Bytecode Compiler
 *
 * Compiles Python source to PikaPython bytecode in the browser.
 * Target: wasm32-unknown-unknown
 */

#include <stdint.h>
#include <stddef.h>

// Simple bump allocator for WASM
static uint8_t heap[1024 * 1024];  // 1MB heap
static size_t heap_ptr = 0;

void* malloc(size_t size) {
    size = (size + 7) & ~7;  // Align to 8 bytes
    if (heap_ptr + size > sizeof(heap)) return NULL;
    void* ptr = &heap[heap_ptr];
    heap_ptr += size;
    return ptr;
}

void* calloc(size_t n, size_t size) {
    size_t total = n * size;
    void* ptr = malloc(total);
    if (ptr) {
        uint8_t* p = ptr;
        for (size_t i = 0; i < total; i++) p[i] = 0;
    }
    return ptr;
}

void* realloc(void* ptr, size_t size) {
    // Simple implementation: always allocate new
    void* new_ptr = malloc(size);
    if (new_ptr && ptr) {
        // Can't know old size, so this is lossy - but works for growing
        uint8_t* src = ptr;
        uint8_t* dst = new_ptr;
        for (size_t i = 0; i < size; i++) dst[i] = src[i];
    }
    return new_ptr;
}

void free(void* ptr) {
    // Bump allocator doesn't free
    (void)ptr;
}

void abort(void) {
    __builtin_trap();
}

// String functions
size_t strlen(const char* s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

char* strcpy(char* dst, const char* src) {
    char* ret = dst;
    while ((*dst++ = *src++));
    return ret;
}

char* strncpy(char* dst, const char* src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i]; i++) dst[i] = src[i];
    for (; i < n; i++) dst[i] = '\0';
    return dst;
}

int strcmp(const char* a, const char* b) {
    while (*a && *a == *b) { a++; b++; }
    return *(unsigned char*)a - *(unsigned char*)b;
}

int strncmp(const char* a, const char* b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i]) return (unsigned char)a[i] - (unsigned char)b[i];
        if (a[i] == '\0') return 0;
    }
    return 0;
}

char* strcat(char* dst, const char* src) {
    char* ret = dst;
    while (*dst) dst++;
    while ((*dst++ = *src++));
    return ret;
}

char* strchr(const char* s, int c) {
    while (*s) {
        if (*s == c) return (char*)s;
        s++;
    }
    return c == 0 ? (char*)s : NULL;
}

char* strstr(const char* haystack, const char* needle) {
    if (!*needle) return (char*)haystack;
    for (; *haystack; haystack++) {
        const char* h = haystack;
        const char* n = needle;
        while (*h && *n && *h == *n) { h++; n++; }
        if (!*n) return (char*)haystack;
    }
    return NULL;
}

char* strdup(const char* s) {
    size_t len = strlen(s) + 1;
    char* dup = malloc(len);
    if (dup) strcpy(dup, s);
    return dup;
}

void* memset(void* dst, int c, size_t n) {
    uint8_t* d = dst;
    for (size_t i = 0; i < n; i++) d[i] = (uint8_t)c;
    return dst;
}

void* memcpy(void* dst, const void* src, size_t n) {
    uint8_t* d = dst;
    const uint8_t* s = src;
    for (size_t i = 0; i < n; i++) d[i] = s[i];
    return dst;
}

void* memmove(void* dst, const void* src, size_t n) {
    uint8_t* d = dst;
    const uint8_t* s = src;
    if (d < s) {
        for (size_t i = 0; i < n; i++) d[i] = s[i];
    } else {
        for (size_t i = n; i > 0; i--) d[i-1] = s[i-1];
    }
    return dst;
}

int memcmp(const void* a, const void* b, size_t n) {
    const uint8_t* pa = a;
    const uint8_t* pb = b;
    for (size_t i = 0; i < n; i++) {
        if (pa[i] != pb[i]) return pa[i] - pb[i];
    }
    return 0;
}

// Number to string
long strtol(const char* s, char** endptr, int base) {
    long result = 0;
    int neg = 0;
    while (*s == ' ') s++;
    if (*s == '-') { neg = 1; s++; }
    else if (*s == '+') s++;

    if (base == 0) {
        if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) { base = 16; s += 2; }
        else if (s[0] == '0') { base = 8; s++; }
        else base = 10;
    }

    while (*s) {
        int digit;
        if (*s >= '0' && *s <= '9') digit = *s - '0';
        else if (*s >= 'a' && *s <= 'z') digit = *s - 'a' + 10;
        else if (*s >= 'A' && *s <= 'Z') digit = *s - 'A' + 10;
        else break;
        if (digit >= base) break;
        result = result * base + digit;
        s++;
    }
    if (endptr) *endptr = (char*)s;
    return neg ? -result : result;
}

long long strtoll(const char* s, char** endptr, int base) {
    return (long long)strtol(s, endptr, base);
}

double strtod(const char* s, char** endptr) {
    double result = 0.0;
    int neg = 0;
    while (*s == ' ') s++;
    if (*s == '-') { neg = 1; s++; }
    else if (*s == '+') s++;

    while (*s >= '0' && *s <= '9') {
        result = result * 10.0 + (*s - '0');
        s++;
    }
    if (*s == '.') {
        s++;
        double frac = 0.1;
        while (*s >= '0' && *s <= '9') {
            result += (*s - '0') * frac;
            frac *= 0.1;
            s++;
        }
    }
    if (endptr) *endptr = (char*)s;
    return neg ? -result : result;
}

// Minimal printf for WASM (just returns 0, we don't print)
int printf(const char* fmt, ...) { (void)fmt; return 0; }
int fprintf(void* f, const char* fmt, ...) { (void)f; (void)fmt; return 0; }
int sprintf(char* s, const char* fmt, ...) { (void)s; (void)fmt; return 0; }
int snprintf(char* s, size_t n, const char* fmt, ...) {
    (void)s; (void)n; (void)fmt;
    if (n > 0) s[0] = '\0';
    return 0;
}

// Stub FILE operations
typedef void FILE;
FILE* fopen(const char* path, const char* mode) { (void)path; (void)mode; return NULL; }
int fclose(FILE* f) { (void)f; return 0; }
size_t fread(void* ptr, size_t size, size_t n, FILE* f) { (void)ptr; (void)size; (void)n; (void)f; return 0; }
size_t fwrite(const void* ptr, size_t size, size_t n, FILE* f) { (void)ptr; (void)size; (void)n; (void)f; return 0; }
int fseek(FILE* f, long offset, int whence) { (void)f; (void)offset; (void)whence; return 0; }
long ftell(FILE* f) { (void)f; return 0; }

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

// Output buffer (shared with JS)
static uint8_t output_buffer[64 * 1024];  // 64KB output buffer
static uint32_t output_size = 0;

// Export functions
__attribute__((export_name("get_output_ptr")))
uint8_t* get_output_ptr(void) {
    return output_buffer;
}

__attribute__((export_name("get_output_size")))
uint32_t get_output_size(void) {
    return output_size;
}

__attribute__((export_name("reset_heap")))
void reset_heap(void) {
    heap_ptr = 0;
    output_size = 0;
}

__attribute__((export_name("compile")))
int compile(const char* source) {
    // Reset allocator
    heap_ptr = 0;
    output_size = 0;

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

    if (total_size > sizeof(output_buffer)) {
        return -2;  // Output too large
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

// Dummy main for linking
int main(void) { return 0; }
