/*
 * PikaPlatform_sbf.h - Solana BPF Platform declarations
 *
 * This header declares all platform functions implemented in PikaPlatform_sbf.c
 * Include this header when using separate compilation units.
 */

#ifndef PIKA_PLATFORM_SBF_H
#define PIKA_PLATFORM_SBF_H

#include <stddef.h>
#include <stdint.h>

/* Solana SDK */
#include <solana_sdk.h>
#include <sol/return_data.h>

/*
 * =============================================================================
 * Memory allocation (bump allocator)
 * =============================================================================
 */
void* pika_platform_malloc(size_t size);
void* pika_platform_calloc(size_t n, size_t s);
void* pika_platform_realloc(void* ptr, size_t size);
void pika_platform_free(void* ptr);

/* Standard library wrappers */
void* malloc(size_t size);
void free(void* ptr);

/*
 * =============================================================================
 * Memory operations
 * =============================================================================
 */
void* pika_platform_memset(void* m, int c, size_t n);
void* pika_platform_memcpy(void* d, const void* s, size_t n);
int pika_platform_memcmp(const void* a, const void* b, size_t n);
void* pika_platform_memmove(void* d, void* s, size_t n);

/* Standard library wrappers */
void* memset(void* m, int c, size_t n);
void* memcpy(void* d, const void* s, size_t n);
int memcmp(const void* a, const void* b, size_t n);
void* memmove(void* d, const void* s, size_t n);

/*
 * =============================================================================
 * String operations
 * =============================================================================
 */
size_t strlen(const char* s);
char* strcpy(char* dst, const char* src);
char* strncpy(char* dst, const char* src, size_t n);
int strcmp(const char* a, const char* b);
int strncmp(const char* a, const char* b, size_t n);
char* strdup(const char* src);
char* strndup(const char* src, size_t n);
char* strchr(const char* s, int c);
char* strrchr(const char* s, int c);
char* strstr(const char* haystack, const char* needle);
char* strcat(char* dst, const char* src);

/* String to number conversions */
long long strtoll(const char* nptr, char** endptr, int base);
long strtol(const char* nptr, char** endptr, int base);
double strtod(const char* nptr, char** endptr);
int strGetSizeUtf8(char* str);

/*
 * =============================================================================
 * I/O stubs
 * =============================================================================
 */
typedef void FILE;

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

int puts(const char* s);
int fflush(void* stream);
void abort(void);
int pika_platform_putchar(char ch);
void pika_putchar(char c);
int pika_platform_fflush(void* stream);

FILE* pika_platform_fopen(const char* filename, const char* modes);
int pika_platform_fclose(FILE* stream);
size_t pika_platform_fwrite(const void* ptr, size_t size, size_t n, FILE* stream);
size_t pika_platform_fread(void* ptr, size_t size, size_t n, FILE* stream);
int pika_platform_fseek(FILE* stream, long offset, int whence);
long pika_platform_ftell(FILE* stream);

/*
 * =============================================================================
 * Printf implementations (variadic, not va_list)
 * =============================================================================
 */
int _pika_sprintf_impl5(char* buff, const char* fmt, intptr_t a1, intptr_t a2, intptr_t a3, intptr_t a4);
int _pika_sprintf_impl_variadic(char* buff, const char* fmt, intptr_t a1, intptr_t a2, intptr_t a3, intptr_t a4, intptr_t a5, intptr_t a6, intptr_t a7, intptr_t a8);

void _pika_platform_printf_variadic(const char* fmt, intptr_t a1, intptr_t a2, intptr_t a3, intptr_t a4, intptr_t a5, intptr_t a6, intptr_t a7, intptr_t a8);
void _pika_platform_printf_impl1(const char* fmt);
void _pika_platform_printf_impl2(const char* fmt, intptr_t a1);
void _pika_platform_printf_impl3(const char* fmt, intptr_t a1, intptr_t a2);
void _pika_platform_printf_impl4(const char* fmt, intptr_t a1, intptr_t a2, intptr_t a3);

int _pika_printf_impl1(const char* fmt);
int _pika_printf_impl2(const char* fmt, intptr_t a1);
int _pika_printf_impl3(const char* fmt, intptr_t a1, intptr_t a2);
int _pika_printf_impl4(const char* fmt, intptr_t a1, intptr_t a2, intptr_t a3);

/* snprintf/sprintf with fixed args */
int snprintf(char* str, size_t size, const char* format, ...);
int sprintf(char* str, const char* format, ...);

/* Forward declaration for Args type */
struct Args;
typedef struct Args Args;

char* _strsFormat_variadic(void* buffs_void, uint16_t buffSize, const char* fmt, intptr_t arg1, intptr_t arg2);

/*
 * =============================================================================
 * PikaObj utilities
 * =============================================================================
 */
struct PikaObj;
typedef struct PikaObj PikaObj;

void _obj_setSysOut_variadic(PikaObj* self, char* fmt, intptr_t a1, intptr_t a2, intptr_t a3, intptr_t a4);

/* Custom print implementation for SBF */
void _pika_sbf_print(PikaObj* self, Args* args);

/* Builtin constructors */
PikaObj* New_builtins(Args* args);
PikaObj* New_builtins_object(Args* args);

/*
 * =============================================================================
 * Platform stubs
 * =============================================================================
 */
int64_t pika_platform_get_tick(void);
void pika_platform_sleep_ms(uint32_t ms);
void pika_platform_sleep_us(uint32_t us);
void pika_platform_disable_irq_handle(void);
void pika_platform_enable_irq_handle(void);
uint8_t pika_is_locked_pikaMemory(void);
void pika_platform_wait(void);
void pika_platform_error_handle(void);
void pika_platform_panic_handle(void);

/* Threading stubs */
typedef void pika_platform_thread_t;
pika_platform_thread_t* pika_platform_thread_init(const char* name, void (*entry)(void*), void* const param, unsigned int stack_size, unsigned int priority, unsigned int tick);
void pika_platform_thread_exit(pika_platform_thread_t* thread);
void pika_platform_thread_destroy(pika_platform_thread_t* thread);
void pika_platform_thread_start(pika_platform_thread_t* thread);
void pika_platform_thread_stop(pika_platform_thread_t* thread);
void pika_platform_thread_yield(void);
uint64_t pika_platform_thread_self(void);

/* Filesystem stubs */
char* pika_platform_getcwd(char* buf, size_t size);
int pika_platform_chdir(const char* path);
int pika_platform_rmdir(const char* pathname);
int pika_platform_mkdir(const char* pathname, int mode);
char* pika_platform_realpath(const char* path, char* resolved_path);
int pika_platform_path_exists(const char* path);
int pika_platform_path_isdir(const char* path);
int pika_platform_path_isfile(const char* path);
int pika_platform_remove(const char* pathname);
int pika_platform_rename(const char* oldpath, const char* newpath);
char** pika_platform_listdir(const char* path, int* count);

/* Shell/REPL stubs */
struct ShellConfig;
struct ShellHistory;
typedef struct ShellConfig ShellConfig;
typedef struct ShellHistory ShellHistory;

void _do_pikaScriptShell(PikaObj* self, ShellConfig* cfg);
void pika_platform_clear(void);
char pika_platform_getchar(void);
void pika_platform_reboot(void);
int pika_platform_repl_recv(uint8_t* buff, size_t size, uint32_t timeout);
void shHistory_destroy(ShellHistory* self);

/* Additional stubs */
int PikaStdData_FILEIO_init(PikaObj* self, char* path, char* mode);

/* Variadic constructor stubs */
void* _pika_list_new(int n);
void* _pika_tuple_new(int n);
void* _pika_dict_new(int n);

/* Object constructors */
PikaObj* New_PikaStdData_ByteArray(Args* args);
PikaObj* New_PikaStdData_String(Args* args);
PikaObj* New_PikaStdLib_SysObj(Args* args);
PikaObj* New_PikaStdData_FILEIO(Args* args);
PikaObj* New_builtins_RangeObj(Args* args);
PikaObj* New_builtins_StringObj(Args* args);

#endif /* PIKA_PLATFORM_SBF_H */
