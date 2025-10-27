#pragma once
/**
 * @brief Solana string and memory system calls and utilities
 */

#include <sol/constants.h>
#include <sol/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Solana memory syscalls */
uint64_t sol_memcpy_(uint8_t *dst, const uint8_t *src, uint64_t n);
uint64_t sol_memmove_(uint8_t *dst, const uint8_t *src, uint64_t n);
uint64_t sol_memcmp_(const uint8_t *s1, const uint8_t *s2, uint64_t n, int32_t *result);
uint64_t sol_memset_(uint8_t *s, uint8_t c, uint64_t n);

/**
 * Copies memory (uses syscall)
 */
static void *sol_memcpy(void *dst, const void *src, int len) {
  sol_memcpy_((uint8_t *)dst, (const uint8_t *)src, (uint64_t)len);
  return dst;
}

/**
 * Compares memory (uses syscall)
 */
static int sol_memcmp(const void *s1, const void *s2, int n) {
  int32_t result = 0;
  sol_memcmp_((const uint8_t *)s1, (const uint8_t *)s2, (uint64_t)n, &result);
  return result;
}

/**
 * Fill a byte string with a byte value (uses syscall)
 */
static void *sol_memset(void *b, int c, size_t len) {
  sol_memset_((uint8_t *)b, (uint8_t)c, (uint64_t)len);
  return b;
}

/**
 * Find length of string
 */
static size_t sol_strlen(const char *s) {
  size_t len = 0;
  while (*s) {
    len++;
    s++;
  }
  return len;
}

/**
 * Alloc zero-initialized memory
 */
static void *sol_calloc(size_t nitems, size_t size) {
  // Bump allocator
  uint64_t* pos_ptr = (uint64_t*)HEAP_START_ADDRESS;

  uint64_t pos = *pos_ptr;
  if (pos == 0) {
      /** First time, set starting position */
      pos = HEAP_START_ADDRESS + HEAP_LENGTH;
  }

  uint64_t bytes = (uint64_t)(nitems * size);
  if (size == 0 ||
      !(nitems == 0 || size == 0) &&
      !(nitems == bytes / size)) {
    /** Overflow */
    return NULL;
  }
  if (pos < bytes) {
    /** Saturated */
    pos = 0;
  } else {
    pos -= bytes;
  }

  uint64_t align = size;
  align--;
  align |= align >> 1;
  align |= align >> 2;
  align |= align >> 4;
  align |= align >> 8;
  align |= align >> 16;
  align |= align >> 32;
  align++;
  pos &= ~(align - 1);
  if (pos < HEAP_START_ADDRESS + sizeof(uint8_t*)) {
      return NULL;
  }
  *pos_ptr = pos;
  return (void*)pos;
}

/**
 * Deallocates the memory previously allocated by sol_calloc
 */
static void sol_free(void *ptr) {
  // I'm a bump allocator, I don't free
}

#ifdef __cplusplus
}
#endif

/**@}*/
