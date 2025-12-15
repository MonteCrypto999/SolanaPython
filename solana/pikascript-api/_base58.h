/*
 * base58 module - Base58 encoding/decoding
 *
 * Functions:
 *   b58decode(s) -> bytes  - Decode base58 string to bytes
 *   b58encode(data) -> str - Encode bytes to base58 string
 */

#ifndef _BASE58_H
#define _BASE58_H

#include "../../src/PikaObj.h"

/* Decode base58 string to bytes */
Arg* _base58_b58decode(PikaObj* self, char* s);

/* Encode bytes to base58 string */
char* _base58_b58encode(PikaObj* self, Arg* data);

#endif /* _BASE58_H */
