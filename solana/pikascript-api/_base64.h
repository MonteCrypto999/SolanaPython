/*
 * _base64.h - Base64 module header for Solana SBF
 */

#ifndef ___base64__H
#define ___base64__H

#include "../../src/PikaObj.h"

PikaObj *New__base64(Args *args);

/* Encode bytes to base64 string */
char* _base64_b64encode(PikaObj *self, Arg* data);

/* Decode base64 string to bytes */
Arg* _base64_b64decode(PikaObj *self, char* s);

#endif
