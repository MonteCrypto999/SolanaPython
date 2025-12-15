/*
 * solana_vfs.c - Virtual Filesystem for Solana Accounts
 *
 * Implementation of FILE-like API backed by Solana account data.
 */

#include "solana_vfs.h"
#include <solana_sdk.h>

/*
 * =============================================================================
 * Heap Layout for VFS
 * =============================================================================
 * We reserve space in the Solana heap (starting at 0x300000000) for VFS data.
 * This extends the existing layout from PikaPlatform_sbf.c:
 *
 * Bytes 0-7:       heap position pointer (existing)
 * Bytes 8-1031:    print line buffer (existing)
 * Bytes 1032-1039: VFS accounts pointer
 * Bytes 1040-1047: VFS accounts count
 * Bytes 1048-1559: VFS file table (8 entries × 64 bytes)
 * Bytes 1560+:     bump allocator space
 */

#define VFS_HEAP_START      0x300000000ULL
#define VFS_ACCOUNTS_PTR    (VFS_HEAP_START + 1032)
#define VFS_ACCOUNTS_COUNT  (VFS_HEAP_START + 1040)
#define VFS_FILE_TABLE      (VFS_HEAP_START + 1048)
#define VFS_FILE_ENTRY_SIZE 64  /* Padded for alignment */

/* SEEK constants */
#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif

/*
 * =============================================================================
 * Base58 Alphabet and Decoding
 * =============================================================================
 */

static const char B58_ALPHABET[] = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

/* Reverse lookup table: ASCII char -> base58 value (-1 = invalid) */
static const int8_t B58_DECODE_MAP[128] = {
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 0x00-0x0F */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 0x10-0x1F */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 0x20-0x2F (space, punctuation) */
    -1, 0, 1, 2, 3, 4, 5, 6, 7, 8,-1,-1,-1,-1,-1,-1,  /* 0x30-0x3F (0-9, note: 0 is invalid) */
    -1, 9,10,11,12,13,14,15,16,-1,17,18,19,20,21,-1,  /* 0x40-0x4F (A-O, note: I is invalid) */
    22,23,24,25,26,27,28,29,30,31,32,-1,-1,-1,-1,-1,  /* 0x50-0x5F (P-Z) */
    -1,33,34,35,36,37,38,39,40,41,42,43,-1,44,45,46,  /* 0x60-0x6F (a-o, note: l is invalid) */
    47,48,49,50,51,52,53,54,55,56,57,-1,-1,-1,-1,-1,  /* 0x70-0x7F (p-z) */
};

/*
 * Decode base58 string to bytes.
 * Optimized to process 2 chars at a time (58^2 = 3364).
 * Returns number of bytes written, or -1 on error.
 */
int sol_b58_decode(const char* b58, uint8_t* out) {
    if (!b58 || !out) return -1;

    /* Count leading '1's (they become leading zeros) */
    int leading_zeros = 0;
    while (*b58 == '1') {
        leading_zeros++;
        b58++;
    }

    /* Get string length and validate */
    int b58_len = 0;
    for (const char* p = b58; *p; p++) {
        if ((unsigned char)*p >= 128 || B58_DECODE_MAP[(unsigned char)*p] < 0) {
            return -1;  /* Invalid character */
        }
        b58_len++;
    }

    if (b58_len == 0 && leading_zeros == 0) {
        return -1;  /* Empty string */
    }

    /* Use a temporary buffer for computation */
    uint8_t temp[36];  /* 32 bytes + safety margin */
    for (int i = 0; i < 36; i++) temp[i] = 0;

    int temp_len = 0;
    int i = 0;

    /* Process pairs of characters (58^2 = 3364) for efficiency */
    while (i + 1 < b58_len) {
        uint32_t carry = B58_DECODE_MAP[(unsigned char)b58[i]] * 58 +
                         B58_DECODE_MAP[(unsigned char)b58[i + 1]];
        i += 2;

        /* Multiply existing value by 3364 and add new digits */
        for (int j = 0; j < temp_len || carry; j++) {
            if (j >= 36) return -1;  /* Overflow */
            uint32_t value = (j < temp_len ? temp[j] : 0) * 3364 + carry;
            temp[j] = value & 0xFF;
            carry = value >> 8;
            if (j >= temp_len) temp_len = j + 1;
        }
    }

    /* Handle odd character if any */
    if (i < b58_len) {
        uint32_t carry = B58_DECODE_MAP[(unsigned char)b58[i]];
        for (int j = 0; j < temp_len || carry; j++) {
            if (j >= 36) return -1;
            uint32_t value = (j < temp_len ? temp[j] : 0) * 58 + carry;
            temp[j] = value & 0xFF;
            carry = value >> 8;
            if (j >= temp_len) temp_len = j + 1;
        }
    }

    /* Reverse the result and add leading zeros */
    int out_len = leading_zeros + temp_len;
    if (out_len > 32) return -1;  /* Too long for pubkey */

    for (int j = 0; j < leading_zeros; j++) {
        out[j] = 0;
    }
    for (int j = 0; j < temp_len; j++) {
        out[leading_zeros + j] = temp[temp_len - 1 - j];
    }

    return out_len;
}

/*
 * Encode bytes to base58 string.
 * Returns length of encoded string, or -1 on error.
 */
int sol_b58_encode(const uint8_t* data, size_t len, char* out) {
    if (!data || !out || len == 0) return -1;

    /* Count leading zeros */
    int leading_zeros = 0;
    while (leading_zeros < (int)len && data[leading_zeros] == 0) {
        leading_zeros++;
    }

    /* Use temporary buffer for base58 digits */
    uint8_t temp[64];
    int temp_len = 0;

    /* Convert bytes to base58 */
    for (int i = leading_zeros; i < (int)len; i++) {
        int carry = data[i];

        for (int j = 0; j < temp_len || carry; j++) {
            if (j >= 64) return -1;

            int value = (j < temp_len ? temp[j] : 0) * 256 + carry;
            temp[j] = value % 58;
            carry = value / 58;

            if (j >= temp_len) temp_len = j + 1;
        }
    }

    /* Build output string */
    int pos = 0;

    /* Leading '1's for each leading zero byte */
    for (int i = 0; i < leading_zeros; i++) {
        out[pos++] = '1';
    }

    /* Base58 digits in reverse order */
    for (int i = temp_len - 1; i >= 0; i--) {
        out[pos++] = B58_ALPHABET[temp[i]];
    }

    out[pos] = '\0';
    return pos;
}

/*
 * =============================================================================
 * VFS Internal Helpers
 * =============================================================================
 */

static SolVfsAccountInfo** get_accounts_ptr(void) {
    return (SolVfsAccountInfo**)VFS_ACCOUNTS_PTR;
}

static uint64_t* get_accounts_count(void) {
    return (uint64_t*)VFS_ACCOUNTS_COUNT;
}

static SolVirtualFile* get_file_table(void) {
    return (SolVirtualFile*)VFS_FILE_TABLE;
}

static SolVirtualFile* get_file_entry(int fd) {
    if (fd < 0 || fd >= SOL_VFS_MAX_OPEN_FILES) return (SolVirtualFile*)0;
    return &get_file_table()[fd];
}

/* Compare two pubkeys */
static int pubkey_equal(const uint8_t* a, const uint8_t* b) {
    for (int i = 0; i < SOL_PUBKEY_SIZE; i++) {
        if (a[i] != b[i]) return 0;
    }
    return 1;
}

/* Find an account by pubkey in the accounts array */
static SolVfsAccountInfo* find_account(const uint8_t* pubkey) {
    SolVfsAccountInfo* accounts = *get_accounts_ptr();
    uint64_t count = *get_accounts_count();

    if (!accounts || count == 0) return (SolVfsAccountInfo*)0;

    for (uint64_t i = 0; i < count; i++) {
        if (accounts[i].pubkey && pubkey_equal(accounts[i].pubkey, pubkey)) {
            return &accounts[i];
        }
    }
    return (SolVfsAccountInfo*)0;
}

/* Find a free slot in the file table */
static int find_free_slot(void) {
    SolVirtualFile* ft = get_file_table();
    for (int i = 0; i < SOL_VFS_MAX_OPEN_FILES; i++) {
        if (!ft[i].is_open) return i;
    }
    return -1;
}

/* Check if mode requests write access */
static int mode_wants_write(const char* mode) {
    if (!mode) return 0;
    for (const char* p = mode; *p; p++) {
        if (*p == 'w' || *p == 'a' || *p == '+') return 1;
    }
    return 0;
}

/*
 * =============================================================================
 * VFS Public API
 * =============================================================================
 */

void sol_vfs_init(SolVfsAccountInfo* accounts, uint64_t num_accounts) {
    /* Store accounts pointer and count */
    *get_accounts_ptr() = accounts;
    *get_accounts_count() = num_accounts;

    /* Clear file table */
    SolVirtualFile* ft = get_file_table();
    for (int i = 0; i < SOL_VFS_MAX_OPEN_FILES; i++) {
        ft[i].is_open = 0;
        ft[i].data = (uint8_t*)0;
        ft[i].size = 0;
        ft[i].position = 0;
        ft[i].is_writable = 0;
    }

}

int sol_vfs_open(const char* pubkey_b58, const char* mode) {
    if (!pubkey_b58 || !mode) return -1;

    /* Decode base58 pubkey */
    uint8_t pubkey[SOL_PUBKEY_SIZE];
    int decoded_len = sol_b58_decode(pubkey_b58, pubkey);
    if (decoded_len != SOL_PUBKEY_SIZE) {
        sol_log("VFS open: invalid pubkey");
        return -1;
    }

    /* Find the account */
    SolVfsAccountInfo* account = find_account(pubkey);
    if (!account) {
        sol_log("VFS open: account not found");
        return -1;
    }

    /* Check write permission */
    int wants_write = mode_wants_write(mode);
    if (wants_write && !account->is_writable) {
        sol_log("VFS open: write requested but account not writable");
        return -1;
    }

    /* Find free slot */
    int fd = find_free_slot();
    if (fd < 0) {
        sol_log("VFS open: no free file slots");
        return -1;
    }

    /* Initialize file entry */
    SolVirtualFile* f = get_file_entry(fd);
    f->data = account->data;
    f->size = account->data_len;
    f->position = 0;
    f->is_writable = account->is_writable;
    f->is_open = 1;

    /* Copy pubkey */
    for (int i = 0; i < SOL_PUBKEY_SIZE; i++) {
        f->pubkey[i] = pubkey[i];
    }

    return fd;
}

int sol_vfs_open_by_index(uint64_t account_index, const char* mode) {
    SolVfsAccountInfo* accounts = *get_accounts_ptr();
    uint64_t count = *get_accounts_count();

    if (!accounts || account_index >= count) {
        sol_log("VFS open_by_index: invalid index");
        return -1;
    }

    SolVfsAccountInfo* account = &accounts[account_index];

    /* Check write permission */
    int wants_write = mode_wants_write(mode);
    if (wants_write && !account->is_writable) {
        sol_log("VFS open_by_index: write requested but account not writable");
        return -1;
    }

    /* Find free slot */
    int fd = find_free_slot();
    if (fd < 0) {
        sol_log("VFS open_by_index: no free file slots");
        return -1;
    }

    /* Initialize file entry */
    SolVirtualFile* f = get_file_entry(fd);
    f->data = account->data;
    f->size = account->data_len;
    f->position = 0;
    f->is_writable = account->is_writable;
    f->is_open = 1;

    /* Copy pubkey */
    if (account->pubkey) {
        for (int i = 0; i < SOL_PUBKEY_SIZE; i++) {
            f->pubkey[i] = account->pubkey[i];
        }
    }

    return fd;
}

size_t sol_vfs_read(void* buf, size_t size, size_t count, int fd) {
    SolVirtualFile* f = get_file_entry(fd);
    if (!f || !f->is_open || !buf) return 0;

    size_t total_bytes = size * count;
    if (total_bytes == 0) return 0;

    /* Check how much we can read */
    if (f->position >= f->size) return 0;  /* EOF */

    size_t available = f->size - f->position;
    if (total_bytes > available) {
        total_bytes = available;
    }

    /* Copy data */
    uint8_t* dst = (uint8_t*)buf;
    uint8_t* src = f->data + f->position;
    for (size_t i = 0; i < total_bytes; i++) {
        dst[i] = src[i];
    }

    f->position += total_bytes;

    /* Return number of complete elements read */
    return total_bytes / size;
}

size_t sol_vfs_write(const void* buf, size_t size, size_t count, int fd) {
    SolVirtualFile* f = get_file_entry(fd);
    if (!f || !f->is_open || !buf) return 0;
    if (!f->is_writable) return 0;

    size_t total_bytes = size * count;
    if (total_bytes == 0) return 0;

    /* Check how much we can write */
    if (f->position >= f->size) return 0;  /* At end */

    size_t available = f->size - f->position;
    if (total_bytes > available) {
        total_bytes = available;
    }

    /* Copy data */
    const uint8_t* src = (const uint8_t*)buf;
    uint8_t* dst = f->data + f->position;
    for (size_t i = 0; i < total_bytes; i++) {
        dst[i] = src[i];
    }

    f->position += total_bytes;

    /* Return number of complete elements written */
    return total_bytes / size;
}

int sol_vfs_seek(int fd, long offset, int whence) {
    SolVirtualFile* f = get_file_entry(fd);
    if (!f || !f->is_open) return -1;

    long new_pos;
    switch (whence) {
        case SEEK_SET:
            new_pos = offset;
            break;
        case SEEK_CUR:
            new_pos = (long)f->position + offset;
            break;
        case SEEK_END:
            new_pos = (long)f->size + offset;
            break;
        default:
            return -1;
    }

    if (new_pos < 0) new_pos = 0;
    if ((uint64_t)new_pos > f->size) new_pos = (long)f->size;

    f->position = (uint64_t)new_pos;
    return 0;
}

long sol_vfs_tell(int fd) {
    SolVirtualFile* f = get_file_entry(fd);
    if (!f || !f->is_open) return -1;
    return (long)f->position;
}

int sol_vfs_close(int fd) {
    SolVirtualFile* f = get_file_entry(fd);
    if (!f || !f->is_open) return -1;

    f->is_open = 0;
    f->data = (uint8_t*)0;
    f->size = 0;
    f->position = 0;

    return 0;
}

const uint8_t* sol_vfs_get_pubkey(int fd) {
    SolVirtualFile* f = get_file_entry(fd);
    if (!f || !f->is_open) return (uint8_t*)0;
    return f->pubkey;
}

uint64_t sol_vfs_get_size(int fd) {
    SolVirtualFile* f = get_file_entry(fd);
    if (!f || !f->is_open) return 0;
    return f->size;
}

int sol_vfs_is_writable(int fd) {
    SolVirtualFile* f = get_file_entry(fd);
    if (!f || !f->is_open) return 0;
    return f->is_writable;
}
