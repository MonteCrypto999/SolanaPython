/*
 * pika_sbf_config.h - Unified configuration for PikaPython on Solana SBF
 *
 * This single header provides all necessary overrides and declarations
 * for building PikaPython on the Solana SBF runtime.
 *
 * Include order managed by build system via -include flag.
 */

#ifndef PIKA_SBF_CONFIG_H
#define PIKA_SBF_CONFIG_H

#include <stddef.h>
#include <stdint.h>

/* =============================================================================
 * PikaPython feature configuration
 * ============================================================================= */

#define PIKA_BUILTIN_STRUCT_ENABLE 1

/* Enable custom hook for unused stack arguments (REPL-style return data) */
#define PIKA_HOOK_UNUSED_STACK_ARG_OVERRIDE 1

/* =============================================================================
 * Standard definitions
 * ============================================================================= */

#ifndef NULL
#define NULL ((void*)0)
#endif

#ifndef offsetof
#define offsetof(type, member) ((size_t) &(((type*)0)->member))
#endif

/* =============================================================================
 * Solana SDK logging
 * ============================================================================= */

/* sol_log_ is the syscall */
extern void sol_log_(const char*, uint64_t);

/* Provide sol_log as an inline function */
static inline void sol_log(const char* msg) {
    uint64_t len = 0;
    if (msg) { while (msg[len]) len++; }
    sol_log_(msg, len);
}

/* =============================================================================
 * FILE type stub
 * ============================================================================= */

typedef void FILE;

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

/* =============================================================================
 * String functions (implemented in PikaPlatform_sbf.c)
 * ============================================================================= */

size_t strlen(const char* s);
char* strcpy(char* dst, const char* src);
char* strcat(char* dst, const char* src);
int strcmp(const char* a, const char* b);
char* strncpy(char* dst, const char* src, size_t n);
char* strchr(const char* s, int c);
char* strstr(const char* haystack, const char* needle);
int strncmp(const char* a, const char* b, size_t n);
char* strdup(const char* s);
char* strndup(const char* s, size_t n);

/* =============================================================================
 * Memory functions (implemented in PikaPlatform_sbf.c)
 * ============================================================================= */

void* memset(void* dst, int c, size_t n);
void* memcpy(void* dst, const void* src, size_t n);
void* memmove(void* dst, const void* src, size_t n);
int memcmp(const void* a, const void* b, size_t n);

/* =============================================================================
 * Memory allocation (implemented in PikaPlatform_sbf.c)
 * ============================================================================= */

void* malloc(size_t n);
void* calloc(size_t n, size_t sz);
void* realloc(void* p, size_t n);
void free(void* p);
void abort(void);

/* =============================================================================
 * String to number conversions (implemented in PikaPlatform_sbf.c)
 * ============================================================================= */

long long strtoll(const char* nptr, char** endptr, int base);
long strtol(const char* nptr, char** endptr, int base);
double strtod(const char* nptr, char** endptr);

/* =============================================================================
 * printf/sprintf family (implemented in PikaPlatform_sbf.c)
 * ============================================================================= */

int snprintf(char* str, size_t size, const char* format, ...);
int sprintf(char* str, const char* format, ...);

void pika_putchar(char c);

/* =============================================================================
 * Variadic function overrides for SBF
 *
 * SBF cannot handle va_list/va_start/va_arg/va_end/va_copy.
 * These macros provide fixed-argument replacements.
 * ============================================================================= */

#ifdef PIKA_SOLANA_SBF

/* Core sprintf implementation - supports up to 4 format args */
int _pika_sprintf_impl5(char* buff, const char* fmt, intptr_t a1, intptr_t a2, intptr_t a3, intptr_t a4);

/* snprintf implementations with varying argument counts */
int _pika_snprintf_impl1(char* buff, size_t size, const char* fmt);
int _pika_snprintf_impl2(char* buff, size_t size, const char* fmt, intptr_t a1);
int _pika_snprintf_impl3(char* buff, size_t size, const char* fmt, intptr_t a1, intptr_t a2);

/* Float to string helper for SBF */
int _pika_float_to_string(float f, char* buf, int buf_size);

/* Printf implementations with varying argument counts */
void _pika_platform_printf_variadic(const char* fmt,
    intptr_t a1, intptr_t a2, intptr_t a3, intptr_t a4,
    intptr_t a5, intptr_t a6, intptr_t a7, intptr_t a8);

int _pika_sprintf_impl_variadic(char* buff, const char* fmt,
    intptr_t a1, intptr_t a2, intptr_t a3, intptr_t a4,
    intptr_t a5, intptr_t a6, intptr_t a7, intptr_t a8);

/* strsFormat for buffer management */
char* _strsFormat_variadic(void* buffs, uint16_t buffSize, const char* fmt, intptr_t a1, intptr_t a2);

/* obj_setSysOut for PikaObj output */
struct PikaObj;
void _obj_setSysOut_variadic(struct PikaObj* self, char* fmt,
    intptr_t a1, intptr_t a2, intptr_t a3, intptr_t a4);

/* -----------------------------------------------------------------------------
 * va_list function replacements via macros
 * ----------------------------------------------------------------------------- */

/* pika_pvsprintf - allocates buffer and formats string */
#define pika_pvsprintf(buff_ptr, fmt, args) \
    _pika_pvsprintf_sbf_macro(buff_ptr, fmt)

static inline int _pika_pvsprintf_sbf_macro(char** buff, const char* fmt) {
    extern void* pika_platform_malloc(size_t);
    size_t len = strlen(fmt);
    *buff = (char*)pika_platform_malloc(len + 1);
    if (!*buff) return -1;
    strcpy(*buff, fmt);
    return (int)len;
}

/* pika_vprintf - prints with va_list */
#define pika_vprintf(fmt, args) \
    _pika_vprintf_sbf_macro(fmt)

static inline int _pika_vprintf_sbf_macro(const char* fmt) {
    uint64_t len = 0;
    if (fmt) { while (fmt[len]) len++; }
    sol_log_(fmt, len);
    return 0;
}

/* pika_platform_vsnprintf - formats to buffer with size limit */
#define pika_platform_vsnprintf(buff, size, fmt, args) \
    _pika_platform_vsnprintf_sbf_macro(buff, size, fmt)

static inline int _pika_platform_vsnprintf_sbf_macro(char* buff, size_t size, const char* fmt) {
    size_t fmt_len = strlen(fmt);
    if (fmt_len >= size) fmt_len = size - 1;
    strncpy(buff, fmt, fmt_len);
    buff[fmt_len] = '\0';
    return (int)fmt_len;
}

/* vsnprintf - standard library function */
#define vsnprintf(buff, size, fmt, args) \
    _pika_platform_vsnprintf_sbf_macro(buff, size, fmt)

/* _no_buff_vprintf - internal helper used by pika_vprintf */
#define _no_buff_vprintf(fmt, args) ((void)0)

/* -----------------------------------------------------------------------------
 * printf/sprintf family macros
 * ----------------------------------------------------------------------------- */

/* pika_snprintf */
int pika_snprintf(char* buff, size_t size, const char* fmt, ...);

/* pika_sprintf - redirect to our impl */
#define pika_sprintf ((int(*)(char*, const char*, ...))_pika_sprintf_impl_variadic)

/* pika_platform_printf - redirect to our impl */
#ifndef pika_platform_printf
#define pika_platform_printf ((void(*)(const char*, ...))_pika_platform_printf_variadic)
#endif

/* strsFormat - redirect to our impl */
#define strsFormat ((char*(*)(void*, uint16_t, const char*, ...))_strsFormat_variadic)

/* obj_setSysOut - redirect to our impl */
#define obj_setSysOut ((void(*)(struct PikaObj*, char*, ...))_obj_setSysOut_variadic)

#endif /* PIKA_SOLANA_SBF */
#endif /* PIKA_SBF_CONFIG_H */
