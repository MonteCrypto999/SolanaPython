/*
 * _solana.h - Solana module header for Solana SBF
 */

#ifndef ___solana__H
#define ___solana__H

#include "../../src/PikaObj.h"

PikaObj *New__solana(Args *args);

/* Get current slot number from clock sysvar */
int64_t _solana_slot(PikaObj *self);

/* Get current epoch number from clock sysvar */
int64_t _solana_epoch(PikaObj *self);

/* Cross-program invocation
 * program_id: int - account index of target program
 * accounts: list - list of int account indices
 * data: bytes - instruction data
 * Returns: int (0 = success, non-zero = error)
 */
int64_t _solana_cpi(PikaObj *self, int program_id, PikaObj* accounts, Arg* data);

/* Hash functions - return 32-byte hash as bytearray */
Arg* _solana_sha256(PikaObj *self, Arg* data);
Arg* _solana_keccak256(PikaObj *self, Arg* data);
Arg* _solana_blake3(PikaObj *self, Arg* data);

/* Program Derived Address (PDA) functions */
/* create_program_address(seeds: list[bytes], program_id: bytes) -> bytes (32-byte pubkey) */
Arg* _solana_create_program_address(PikaObj *self, PikaObj* seeds, Arg* program_id);

/* try_find_program_address(seeds: list[bytes], program_id: bytes) -> tuple(bytes, int) */
PikaObj* _solana_try_find_program_address(PikaObj *self, PikaObj* seeds, Arg* program_id);

#endif
