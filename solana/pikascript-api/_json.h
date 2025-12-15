/*
 * _json.h - JSON module header for Solana SBF
 */

#ifndef ___json__H
#define ___json__H

#include "../../src/PikaObj.h"

PikaObj *New__json(Args *args);

/* Serialize Python object to JSON string */
char* _json_dumps(PikaObj *self, Arg* obj);

/* Parse JSON string to Python object */
Arg* _json_loads(PikaObj *self, char* s);

#endif
