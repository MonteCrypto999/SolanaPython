/*
 * _struct.h - Struct module header for Solana SBF
 * Binary packing/unpacking (subset of Python struct module)
 */

#ifndef ___struct__H
#define ___struct__H

#include "../../src/PikaObj.h"

PikaObj *New__struct(Args *args);

/* Pack values into bytes according to format string
 * Format: '<' or '>' for endianness, then type codes:
 *   b/B = int8/uint8, h/H = int16/uint16, i/I = int32/uint32, q/Q = int64/uint64
 */
Arg* _struct_pack(PikaObj *self, char* fmt, PikaObj* values);

/* Unpack bytes into tuple according to format string */
PikaObj* _struct_unpack(PikaObj *self, char* fmt, Arg* data);

/* Calculate size of packed data for format string */
int _struct_calcsize(PikaObj *self, char* fmt);

#endif
