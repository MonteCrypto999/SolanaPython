/*
 * _solana_sbf.c - Solana module for Solana SBF
 * Provides access to Solana runtime: slot, epoch, CPI, hashing, PDAs
 */

#include "_solana.h"
#include "../../src/TinyObj.h"

/* get_bytes_from_arg helper defined in pika_python.c */

#ifdef PIKA_SOLANA_SBF
#include <sol/cpi.h>
#include <sol/pubkey.h>

/* SolClock and sol_get_clock_sysvar declared in builtins_sbf.c (included first) */

/* Hash syscalls use SolBytes struct: { ptr, len } */
#include <sol/sha.h>
#include <sol/keccak.h>
/* Note: blake3 syscall not available on mainnet yet */

/* Get slot from clock sysvar */
static int64_t get_clock_slot(void) {
    uint64_t clock[5];  /* slot, epoch_start_ts, epoch, leader_epoch, unix_ts */
    extern uint64_t sol_get_clock_sysvar(void*);
    sol_get_clock_sysvar(clock);
    return (int64_t)clock[0];  /* slot is at index 0 */
}

/* Get epoch from clock sysvar */
static int64_t get_clock_epoch(void) {
    uint64_t clock[5];
    extern uint64_t sol_get_clock_sysvar(void*);
    sol_get_clock_sysvar(clock);
    return (int64_t)clock[2];  /* epoch is at index 2 */
}

/* CPI context accessors (defined in pika_python.c) */
extern SolAccountInfo* pika_get_cpi_accounts(void);
extern uint64_t pika_get_cpi_num_accounts(void);
#endif

/* Get current slot number */
int64_t _solana_slot(PikaObj* self) {
    (void)self;
#ifdef PIKA_SOLANA_SBF
    return (int64_t)get_clock_slot();
#else
    return 0;
#endif
}

/* Get current epoch number */
int64_t _solana_epoch(PikaObj* self) {
    (void)self;
#ifdef PIKA_SOLANA_SBF
    return (int64_t)get_clock_epoch();
#else
    return 0;
#endif
}

/* Cross-program invocation */
int64_t _solana_cpi(PikaObj* self, int program_id, PikaObj* accounts, Arg* data) {
    (void)self;
#ifdef PIKA_SOLANA_SBF
    SolAccountInfo* tx_accounts = pika_get_cpi_accounts();
    uint64_t num_accounts = pika_get_cpi_num_accounts();

    if (tx_accounts == NULL || num_accounts == 0) {
        return -1;
    }

    /* Get data bytes - skip size_t prefix */
    uint8_t* data_ptr = arg_getBytes(data) + sizeof(size_t);
    uint64_t data_len = arg_getBytesSize(data);

    int numCpiAccounts = (int)pikaList_getSize(accounts);

    /* Stack array for account metas (max 16) */
    SolAccountMeta metas[16];

    for (int i = 0; i < numCpiAccounts && i < 16; i++) {
        Arg* elem = pikaList_getArg(accounts, i);
        int idx = 0;
        int is_writable = 0;
        int is_signer = 0;

        if (elem != NULL && arg_isObject(elem)) {
            /* Element is a tuple: (account_idx, is_writable, is_signer) */
            PikaObj* tuple = arg_getPtr(elem);
            idx = (int)pikaList_getInt(tuple, 0);
            is_writable = (int)pikaList_getInt(tuple, 1);
            is_signer = (int)pikaList_getInt(tuple, 2);
        } else {
            /* Element is just an integer index - inherit flags from account */
            idx = (int)pikaList_getInt(accounts, i);
            is_writable = tx_accounts[idx].is_writable;
            is_signer = tx_accounts[idx].is_signer;
        }

        if (idx < 0 || (uint64_t)idx >= num_accounts) {
            return -2;  /* Invalid account index */
        }
        SolAccountInfo* acct = &tx_accounts[idx];
        metas[i].pubkey = acct->key;
        metas[i].is_writable = is_writable;
        metas[i].is_signer = is_signer;
    }

    /* Validate program index */
    if (program_id < 0 || (uint64_t)program_id >= num_accounts) {
        return -3;  /* Invalid program index */
    }

    SolInstruction instruction = {
        .program_id = tx_accounts[program_id].key,
        .accounts = metas,
        .account_len = (uint64_t)numCpiAccounts,
        .data = data_ptr,
        .data_len = data_len
    };

    /* Invoke the program */
    uint64_t result = sol_invoke(&instruction, tx_accounts, (int)num_accounts);
    return (int64_t)result;
#else
    return -1;
#endif
}

/* SHA-256 hash */
Arg* _solana_sha256(PikaObj* self, Arg* data) {
    (void)self;
#ifdef PIKA_SOLANA_SBF
    if (data == NULL) return arg_newNull();

    uint8_t* data_ptr = NULL;
    size_t data_len = 0;

    if (!get_bytes_from_arg(data, &data_ptr, &data_len)) return arg_newNull();

    SolBytes input = { .addr = data_ptr, .len = data_len };
    uint8_t hash[32];
    sol_sha256(&input, 1, hash);

    return arg_newBytes(hash, 32);
#else
    (void)data;
    return arg_newNull();
#endif
}

/* Keccak-256 hash */
Arg* _solana_keccak256(PikaObj* self, Arg* data) {
    (void)self;
#ifdef PIKA_SOLANA_SBF
    if (data == NULL) return arg_newNull();

    uint8_t* data_ptr = NULL;
    size_t data_len = 0;

    if (!get_bytes_from_arg(data, &data_ptr, &data_len)) return arg_newNull();

    SolBytes input = { .addr = data_ptr, .len = data_len };
    uint8_t hash[32];
    sol_keccak256(&input, 1, hash);

    return arg_newBytes(hash, 32);
#else
    (void)data;
    return arg_newNull();
#endif
}

/* Note: BLAKE3 syscall not available on mainnet yet */

/* Create program address from seeds and program ID */
Arg* _solana_create_program_address(PikaObj* self, PikaObj* seeds, Arg* program_id) {
    (void)self;
#ifdef PIKA_SOLANA_SBF
    if (seeds == NULL || program_id == NULL) return arg_newNull();

    /* Get program ID bytes (32 bytes) */
    uint8_t* prog_ptr = NULL;
    size_t prog_len = 0;
    if (!get_bytes_from_arg(program_id, &prog_ptr, &prog_len) || prog_len < 32) {
        return arg_newNull();
    }

    /* Build seeds array using SolSignerSeed structs */
    int num_seeds = pikaList_getSize(seeds);
    SolSignerSeed signer_seeds[16];  /* Max 16 seeds */
    uint8_t seed_data[256];  /* Buffer for seed data */
    size_t data_pos = 0;

    int actual_seeds = 0;
    for (int i = 0; i < num_seeds && i < 16 && data_pos < 240; i++) {
        Arg* seed = pikaList_get(seeds, i);
        if (seed == NULL) continue;

        uint8_t* s_ptr = NULL;
        size_t s_len = 0;
        if (!get_bytes_from_arg(seed, &s_ptr, &s_len)) continue;

        /* Copy seed data to buffer */
        pika_platform_memcpy(seed_data + data_pos, s_ptr, s_len);
        signer_seeds[actual_seeds].addr = seed_data + data_pos;
        signer_seeds[actual_seeds].len = s_len;
        data_pos += s_len;
        actual_seeds++;
    }

    SolPubkey result_pubkey;
    uint64_t result = sol_create_program_address(signer_seeds, actual_seeds,
                                                  (const SolPubkey*)prog_ptr, &result_pubkey);

    if (result != 0) {
        return arg_newNull();  /* Invalid seeds (on curve) */
    }

    return arg_newBytes(result_pubkey.x, 32);
#else
    (void)seeds;
    (void)program_id;
    return arg_newNull();
#endif
}

/* Find program address with bump seed */
PikaObj* _solana_try_find_program_address(PikaObj* self, PikaObj* seeds, Arg* program_id) {
    (void)self;
#ifdef PIKA_SOLANA_SBF
    if (seeds == NULL || program_id == NULL) return NULL;

    /* Get program ID bytes */
    uint8_t* prog_ptr = NULL;
    size_t prog_len = 0;
    if (!get_bytes_from_arg(program_id, &prog_ptr, &prog_len) || prog_len < 32) {
        return NULL;
    }

    /* Build seeds array using SolSignerSeed structs */
    int num_seeds = pikaList_getSize(seeds);
    SolSignerSeed signer_seeds[16];
    uint8_t seed_data[256];
    size_t data_pos = 0;

    int actual_seeds = 0;
    for (int i = 0; i < num_seeds && i < 16 && data_pos < 240; i++) {
        Arg* seed = pikaList_get(seeds, i);
        if (seed == NULL) continue;

        uint8_t* s_ptr = NULL;
        size_t s_len = 0;
        if (!get_bytes_from_arg(seed, &s_ptr, &s_len)) continue;

        pika_platform_memcpy(seed_data + data_pos, s_ptr, s_len);
        signer_seeds[actual_seeds].addr = seed_data + data_pos;
        signer_seeds[actual_seeds].len = s_len;
        data_pos += s_len;
        actual_seeds++;
    }

    SolPubkey result_pubkey;
    uint8_t bump;
    uint64_t result = sol_try_find_program_address(signer_seeds, actual_seeds,
                                                    (const SolPubkey*)prog_ptr, &result_pubkey, &bump);

    if (result != 0) {
        return NULL;
    }

    /* Return as tuple (address_bytes, bump_int) */
    PikaObj* tuple = newNormalObj(New_PikaStdData_Tuple);
    pikaList_init(tuple);  /* Initialize tuple storage */
    pikaList_append(tuple, arg_newBytes(result_pubkey.x, 32));
    pikaList_append(tuple, arg_newInt(bump));

    return tuple;
#else
    (void)seeds;
    (void)program_id;
    return NULL;
#endif
}

/* Method wrappers for binding */

static void _solana_slotMethod(PikaObj* self, Args* args) {
    method_returnInt(args, _solana_slot(self));
}

static void _solana_epochMethod(PikaObj* self, Args* args) {
    method_returnInt(args, _solana_epoch(self));
}

static void _solana_cpiMethod(PikaObj* self, Args* args) {
    int program_id = args_getInt(args, "program_id");

    Arg* aAccounts = args_getArg(args, "accounts");
    PikaObj* accounts = NULL;
    if (aAccounts != NULL && arg_isObject(aAccounts)) {
        accounts = arg_getPtr(aAccounts);
    }

    Arg* data = args_getArg(args, "data");
    if (data == NULL || accounts == NULL) {
        method_returnInt(args, -1);
        return;
    }

    method_returnInt(args, _solana_cpi(self, program_id, accounts, data));
}

static void _solana_sha256Method(PikaObj* self, Args* args) {
    Arg* data = args_getArg(args, "data");
    Arg* result = _solana_sha256(self, data);
    if (result) {
        method_returnArg(args, result);
    } else {
        method_returnArg(args, arg_newNull());
    }
}

static void _solana_keccak256Method(PikaObj* self, Args* args) {
    Arg* data = args_getArg(args, "data");
    Arg* result = _solana_keccak256(self, data);
    if (result) {
        method_returnArg(args, result);
    } else {
        method_returnArg(args, arg_newNull());
    }
}

static void _solana_create_program_addressMethod(PikaObj* self, Args* args) {
    Arg* aSeeds = args_getArg(args, "seeds");
    PikaObj* seeds = NULL;
    if (aSeeds != NULL && arg_isObject(aSeeds)) {
        seeds = arg_getPtr(aSeeds);
    }

    Arg* program_id = args_getArg(args, "program_id");
    if (seeds == NULL || program_id == NULL) {
        method_returnArg(args, arg_newNull());
        return;
    }

    Arg* result = _solana_create_program_address(self, seeds, program_id);
    if (result) {
        method_returnArg(args, result);
    } else {
        method_returnArg(args, arg_newNull());
    }
}

static void _solana_try_find_program_addressMethod(PikaObj* self, Args* args) {
    Arg* aSeeds = args_getArg(args, "seeds");
    PikaObj* seeds = NULL;
    if (aSeeds != NULL && arg_isObject(aSeeds)) {
        seeds = arg_getPtr(aSeeds);
    }

    Arg* program_id = args_getArg(args, "program_id");
    if (seeds == NULL || program_id == NULL) {
        method_returnArg(args, arg_newNull());
        return;
    }

    PikaObj* result = _solana_try_find_program_address(self, seeds, program_id);
    if (result) {
        method_returnObj(args, result);
    } else {
        method_returnArg(args, arg_newNull());
    }
}

/* Constructor */
PikaObj* New__solana(Args* args) {
    PikaObj* self = New_TinyObj(args);

    /* Register methods at runtime */
    class_defineMethod(self, "slot", "", (Method)_solana_slotMethod);
    class_defineMethod(self, "epoch", "", (Method)_solana_epochMethod);
    class_defineMethod(self, "cpi", "program_id,accounts,data", (Method)_solana_cpiMethod);

    /* Hash functions */
    class_defineMethod(self, "sha256", "data", (Method)_solana_sha256Method);
    class_defineMethod(self, "keccak256", "data", (Method)_solana_keccak256Method);
    /* Note: blake3 syscall not available on mainnet yet */

    /* PDA functions */
    class_defineMethod(self, "create_program_address", "seeds,program_id", (Method)_solana_create_program_addressMethod);
    class_defineMethod(self, "find_program_address", "seeds,program_id", (Method)_solana_try_find_program_addressMethod);

    return self;
}
