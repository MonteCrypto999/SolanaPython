/*
 * solana_vfs.h - Virtual Filesystem for Solana Accounts
 *
 * Provides a FILE-like API backed by Solana account data.
 * Each account can be opened by its base58 pubkey address.
 */

#ifndef SOLANA_VFS_H
#define SOLANA_VFS_H

#include <stdint.h>
#include <stddef.h>

/* Maximum number of simultaneously open files */
#define SOL_VFS_MAX_OPEN_FILES 8

/* Pubkey size in bytes */
#define SOL_PUBKEY_SIZE 32

/* Base58 encoded pubkey max length (32 bytes -> ~44 chars + null) */
#define SOL_PUBKEY_B58_MAX 45

/*
 * Virtual file entry - represents an open file backed by account data
 */
typedef struct {
    uint8_t* data;           /* Direct pointer to account data */
    uint64_t size;           /* Account data length */
    uint64_t position;       /* Current read/write position */
    uint8_t is_writable;     /* Whether writes are allowed */
    uint8_t is_open;         /* Whether this slot is in use */
    uint8_t pubkey[SOL_PUBKEY_SIZE]; /* Account pubkey */
} SolVirtualFile;

/*
 * Account info passed from entrypoint (mirrors SolAccountInfo)
 */
typedef struct {
    uint8_t* pubkey;         /* Pointer to 32-byte pubkey */
    uint8_t* data;           /* Pointer to account data */
    uint64_t data_len;       /* Length of account data */
    uint8_t is_writable;     /* Whether account is writable */
    uint8_t is_signer;       /* Whether account is signer */
} SolVfsAccountInfo;

/*
 * Initialize the virtual filesystem with accounts from the transaction.
 * Must be called from entrypoint before any file operations.
 *
 * @param accounts Array of account infos
 * @param num_accounts Number of accounts in the array
 */
void sol_vfs_init(SolVfsAccountInfo* accounts, uint64_t num_accounts);

/*
 * Open a file by account pubkey (base58 encoded).
 *
 * @param pubkey_b58 Base58-encoded pubkey string (null-terminated)
 * @param mode "r" for read, "w" for write, "rw" or "r+" for both
 * @return File descriptor (0+) on success, -1 on error
 *
 * Errors:
 *  - Account not found in transaction
 *  - No free file slots
 *  - Write mode requested but account not writable
 */
int sol_vfs_open(const char* pubkey_b58, const char* mode);

/*
 * Open a file by account index (0-based position in transaction accounts).
 *
 * @param account_index Index of account in transaction
 * @param mode "r" for read, "w" for write, "rw" or "r+" for both
 * @return File descriptor (0+) on success, -1 on error
 */
int sol_vfs_open_by_index(uint64_t account_index, const char* mode);

/*
 * Read data from an open file.
 *
 * @param buf Buffer to read into
 * @param size Size of each element
 * @param count Number of elements to read
 * @param fd File descriptor from sol_vfs_open
 * @return Number of elements read, 0 on EOF or error
 */
size_t sol_vfs_read(void* buf, size_t size, size_t count, int fd);

/*
 * Write data to an open file.
 *
 * @param buf Buffer to write from
 * @param size Size of each element
 * @param count Number of elements to write
 * @param fd File descriptor
 * @return Number of elements written, 0 on error
 */
size_t sol_vfs_write(const void* buf, size_t size, size_t count, int fd);

/*
 * Seek to a position in an open file.
 *
 * @param fd File descriptor
 * @param offset Offset in bytes
 * @param whence SEEK_SET (0), SEEK_CUR (1), or SEEK_END (2)
 * @return 0 on success, -1 on error
 */
int sol_vfs_seek(int fd, long offset, int whence);

/*
 * Get current position in an open file.
 *
 * @param fd File descriptor
 * @return Current position, or -1 on error
 */
long sol_vfs_tell(int fd);

/*
 * Close an open file.
 *
 * @param fd File descriptor
 * @return 0 on success, -1 on error
 */
int sol_vfs_close(int fd);

/*
 * Get the pubkey of an open file (as raw 32 bytes).
 *
 * @param fd File descriptor
 * @return Pointer to 32-byte pubkey, or NULL on error
 */
const uint8_t* sol_vfs_get_pubkey(int fd);

/*
 * Get the size of an open file.
 *
 * @param fd File descriptor
 * @return File size in bytes, or 0 on error
 */
uint64_t sol_vfs_get_size(int fd);

/*
 * Check if a file is writable.
 *
 * @param fd File descriptor
 * @return 1 if writable, 0 if read-only or error
 */
int sol_vfs_is_writable(int fd);

/*
 * Base58 decode a pubkey string to raw bytes.
 *
 * @param b58 Base58-encoded string
 * @param out Output buffer (must be at least 32 bytes)
 * @return Number of bytes written, or -1 on error
 */
int sol_b58_decode(const char* b58, uint8_t* out);

/*
 * Base58 encode raw bytes to a string.
 *
 * @param data Raw bytes to encode
 * @param len Length of data
 * @param out Output buffer (should be at least 45 bytes for pubkey)
 * @return Length of encoded string, or -1 on error
 */
int sol_b58_encode(const uint8_t* data, size_t len, char* out);

#endif /* SOLANA_VFS_H */
