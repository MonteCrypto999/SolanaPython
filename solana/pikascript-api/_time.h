/*
 * _time.h - Time module header for Solana SBF
 */

#ifndef ___time__H
#define ___time__H

#include "../../src/PikaObj.h"

PikaObj *New__time(Args *args);

/* Get current time as unix timestamp (from Solana clock sysvar) */
int64_t _time_time(PikaObj *self);

/* Convert unix timestamp to struct_time (UTC) */
PikaObj* _time_gmtime(PikaObj *self, int64_t secs);

/* Convert unix timestamp to struct_time (local time, UTC+0 on Solana) */
PikaObj* _time_localtime(PikaObj *self, int64_t secs);

/* Convert struct_time tuple to unix timestamp */
int64_t _time_mktime(PikaObj *self, PikaObj* t);

/* Format time as string: "Day Mon DD HH:MM:SS YYYY" */
char* _time_ctime(PikaObj *self, int64_t secs);

/* Format current time as string */
char* _time_asctime(PikaObj *self);

#endif
