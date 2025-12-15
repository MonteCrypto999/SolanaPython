/*
 * _time_sbf.c - Time module for Solana SBF
 * Uses Solana's clock sysvar for unix timestamp
 */

#include "_time.h"
#include "../../src/TinyObj.h"

#ifdef PIKA_SOLANA_SBF
/* SolClock and sol_get_clock_sysvar declared in builtins_sbf.c (included first) */

/* Get unix timestamp from clock sysvar */
static int64_t get_clock_timestamp(void) {
    /* Use raw uint64_t array since SolClock typedef is in builtins scope */
    uint64_t clock[5];  /* slot, epoch_start_ts, epoch, leader_epoch, unix_ts */
    extern uint64_t sol_get_clock_sysvar(void*);
    sol_get_clock_sysvar(clock);
    return (int64_t)clock[4];  /* unix_timestamp is at index 4 */
}
#endif

/* Internal struct_time representation */
typedef struct {
    int tm_sec;    /* seconds [0, 60] */
    int tm_min;    /* minutes [0, 59] */
    int tm_hour;   /* hours [0, 23] */
    int tm_mday;   /* day of month [1, 31] */
    int tm_mon;    /* month [0, 11] */
    int tm_year;   /* year since 1900 */
    int tm_wday;   /* day of week [0, 6] (Sunday=0) */
    int tm_yday;   /* day of year [0, 365] */
    int tm_isdst;  /* daylight savings flag */
} _tm;

/* Constants for time calculations */
#define DAY_SECONDS 86400
#define YEAR_START 1600
#define DAY_OFFSET 135140
#define MAX_DAY 584388
#define DAY_OF_400Y 146097
#define DAY_OF_100Y 36524
#define DAY_OF_4Y 1461
#define DAY_OF_1Y 365

/* Calculate day of week (Sunday=0) */
static int get_weekday(const _tm* tm) {
    int month = tm->tm_mon + 1;
    int year = tm->tm_year;
    int day = tm->tm_mday;
    int w;

    if (month == 1 || month == 2) {
        month += 12;
        year -= 1;
    }
    w = day + 2 * month + 3 * (month + 1) / 5 + year + year / 4 -
        year / 100 + year / 400 + 1;
    return w % 7;
}

/* Convert unix timestamp to struct_time (UTC) */
static void unix_to_tm(int64_t unix_time, _tm* tm) {
    int32_t total_day;
    int32_t extra_second;
    int year_400, year_100, year_4, year_1;
    int february_offset, temp;

    if (unix_time < 0) {
        /* Default to epoch for negative values */
        pika_platform_memset(tm, 0, sizeof(_tm));
        tm->tm_year = 1970;
        tm->tm_mday = 1;
        return;
    }

    total_day = unix_time / DAY_SECONDS;
    extra_second = unix_time - total_day * DAY_SECONDS;
    total_day += DAY_OFFSET;

    if (total_day > MAX_DAY) {
        /* Clamp to year 3200 */
        total_day = MAX_DAY;
    }

    year_400 = (total_day - 366) / DAY_OF_400Y;
    total_day -= year_400 * DAY_OF_400Y;
    year_100 = (total_day - 1) / DAY_OF_100Y;
    total_day -= year_100 * DAY_OF_100Y;
    year_4 = (total_day - 366) / DAY_OF_4Y;
    total_day -= year_4 * DAY_OF_4Y;

    if (year_100 == 4) {
        year_1 = 0;
        february_offset = 1;
    } else if (total_day <= DAY_OF_1Y * 4) {
        year_1 = (total_day - 1) / DAY_OF_1Y;
        total_day -= year_1 * DAY_OF_1Y;
        february_offset = 0;
    } else {
        year_1 = 4;
        total_day -= year_1 * DAY_OF_1Y;
        february_offset = 1;
    }

    tm->tm_year = (year_400 * 400 + year_100 * 100 + year_4 * 4 + year_1) + YEAR_START;
    tm->tm_yday = total_day;

    total_day -= february_offset;
    if (total_day <= 181) {
        if (total_day <= 90) {
            if (total_day <= 59) {
                total_day += february_offset;
                if (total_day <= 31) {
                    temp = 0;
                } else {
                    total_day -= 31;
                    temp = 1;
                }
            } else {
                total_day -= 59;
                temp = 2;
            }
        } else {
            total_day -= 90;
            if (total_day <= 30) {
                temp = 3;
            } else {
                total_day -= 30;
                if (total_day <= 31) {
                    temp = 4;
                } else {
                    total_day -= 31;
                    temp = 5;
                }
            }
        }
    } else {
        total_day -= 181;
        if (total_day <= 92) {
            if (total_day <= 62) {
                if (total_day <= 31) {
                    temp = 6;
                } else {
                    total_day -= 31;
                    temp = 7;
                }
            } else {
                total_day -= 62;
                temp = 8;
            }
        } else {
            total_day -= 92;
            if (total_day <= 61) {
                if (total_day <= 31) {
                    temp = 9;
                } else {
                    total_day -= 31;
                    temp = 10;
                }
            } else {
                total_day -= 61;
                temp = 11;
            }
        }
    }

    tm->tm_mon = temp;
    tm->tm_mday = total_day;

    temp = extra_second / 3600;
    tm->tm_hour = temp;
    extra_second = extra_second - temp * 3600;

    temp = extra_second / 60;
    tm->tm_min = temp;
    extra_second = extra_second - temp * 60;

    tm->tm_sec = extra_second;
    tm->tm_wday = get_weekday(tm);
    tm->tm_isdst = 0;
}

/* Convert struct_time to unix timestamp */
static int64_t tm_to_unix(const _tm* tm) {
    int32_t total_day, total_leap_year, dyear;
    int february_offset;
    const int month_day[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};

    if (tm->tm_year < 1970) {
        return 0;
    }
    if (tm->tm_year >= 3200) {
        return 0;
    }

    dyear = tm->tm_year - YEAR_START - 1;
    total_leap_year = dyear / 4 - (dyear / 100 - dyear / 400 - 1);
    dyear += 1;
    total_day = dyear * 365 + total_leap_year;
    total_day -= DAY_OFFSET;

    if (((dyear % 4 == 0) && (dyear % 100 != 0)) || (dyear % 400 == 0)) {
        february_offset = 1;
    } else {
        february_offset = 0;
    }

    total_day += month_day[tm->tm_mon] + tm->tm_mday - 1;
    if (tm->tm_mon > 1) {
        total_day += february_offset;
    }

    return (int64_t)total_day * DAY_SECONDS + tm->tm_hour * 3600 +
           tm->tm_min * 60 + tm->tm_sec;
}

/* Get current unix timestamp from Solana clock sysvar */
int64_t _time_time(PikaObj* self) {
    (void)self;
#ifdef PIKA_SOLANA_SBF
    return get_clock_timestamp();
#else
    return 0;
#endif
}

/* struct_time object constructor */
static PikaObj* New_time_struct_time(Args* args) {
    (void)args;
    PikaObj* self = New_TinyObj(NULL);
    return self;
}

/* Create struct_time object from _tm */
static PikaObj* create_struct_time(_tm* tm) {
    PikaObj* obj = newNormalObj(New_time_struct_time);
    if (obj == NULL) return NULL;

    /* Store as tuple-like object with indexed access */
    obj_setInt(obj, "tm_year", tm->tm_year);
    obj_setInt(obj, "tm_mon", tm->tm_mon + 1);  /* Python uses 1-12 */
    obj_setInt(obj, "tm_mday", tm->tm_mday);
    obj_setInt(obj, "tm_hour", tm->tm_hour);
    obj_setInt(obj, "tm_min", tm->tm_min);
    obj_setInt(obj, "tm_sec", tm->tm_sec);
    /* Convert wday: our Sunday=0, Python uses Monday=0 */
    int py_wday = tm->tm_wday - 1;
    if (py_wday < 0) py_wday = 6;
    obj_setInt(obj, "tm_wday", py_wday);
    obj_setInt(obj, "tm_yday", tm->tm_yday);
    obj_setInt(obj, "tm_isdst", tm->tm_isdst);

    return obj;
}

/* Convert unix timestamp to struct_time (UTC) */
PikaObj* _time_gmtime(PikaObj* self, int64_t secs) {
    (void)self;
    _tm tm;
    unix_to_tm(secs, &tm);
    return create_struct_time(&tm);
}

/* Convert unix timestamp to struct_time (local time = UTC on Solana) */
PikaObj* _time_localtime(PikaObj* self, int64_t secs) {
    /* On Solana, local time = UTC */
    return _time_gmtime(self, secs);
}

/* Convert struct_time tuple to unix timestamp */
int64_t _time_mktime(PikaObj* self, PikaObj* t) {
    (void)self;
    _tm tm;

    /* Read from tuple indices: (year, mon, mday, hour, min, sec, wday, yday, isdst) */
    tm.tm_year = pikaList_getInt(t, 0);
    tm.tm_mon = pikaList_getInt(t, 1) - 1;  /* Python uses 1-12, internal uses 0-11 */
    tm.tm_mday = pikaList_getInt(t, 2);
    tm.tm_hour = pikaList_getInt(t, 3);
    tm.tm_min = pikaList_getInt(t, 4);
    tm.tm_sec = pikaList_getInt(t, 5);

    return tm_to_unix(&tm);
}

/* Format time as string: "Day Mon DD HH:MM:SS YYYY" */
char* _time_ctime(PikaObj* self, int64_t secs) {
    static const char* weekday[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    static const char* month[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

    _tm tm;
    unix_to_tm(secs, &tm);

    /* Format: "Wed Jun 09 04:26:40 1993\n" (standard ctime format) */
    char buf[32];
    int pos = 0;

    /* Day name */
    const char* d = weekday[tm.tm_wday];
    buf[pos++] = d[0]; buf[pos++] = d[1]; buf[pos++] = d[2];
    buf[pos++] = ' ';

    /* Month name */
    const char* m = month[tm.tm_mon];
    buf[pos++] = m[0]; buf[pos++] = m[1]; buf[pos++] = m[2];
    buf[pos++] = ' ';

    /* Day of month (space-padded) */
    if (tm.tm_mday < 10) {
        buf[pos++] = ' ';
        buf[pos++] = '0' + tm.tm_mday;
    } else {
        buf[pos++] = '0' + tm.tm_mday / 10;
        buf[pos++] = '0' + tm.tm_mday % 10;
    }
    buf[pos++] = ' ';

    /* HH:MM:SS */
    buf[pos++] = '0' + tm.tm_hour / 10;
    buf[pos++] = '0' + tm.tm_hour % 10;
    buf[pos++] = ':';
    buf[pos++] = '0' + tm.tm_min / 10;
    buf[pos++] = '0' + tm.tm_min % 10;
    buf[pos++] = ':';
    buf[pos++] = '0' + tm.tm_sec / 10;
    buf[pos++] = '0' + tm.tm_sec % 10;
    buf[pos++] = ' ';

    /* Year */
    int y = tm.tm_year;
    buf[pos++] = '0' + (y / 1000) % 10;
    buf[pos++] = '0' + (y / 100) % 10;
    buf[pos++] = '0' + (y / 10) % 10;
    buf[pos++] = '0' + y % 10;
    buf[pos] = '\0';

    return obj_cacheStr(self, buf);
}

/* Format current time as string */
char* _time_asctime(PikaObj* self) {
    return _time_ctime(self, _time_time(self));
}

/* Method wrappers for binding */

static void _time_timeMethod(PikaObj* self, Args* args) {
    method_returnInt(args, _time_time(self));
}

static void _time_gmtimeMethod(PikaObj* self, Args* args) {
    int64_t secs = args_getInt(args, "secs");
    PikaObj* result = _time_gmtime(self, secs);
    if (result) {
        method_returnObj(args, result);
    } else {
        method_returnArg(args, arg_newNone());
    }
}

static void _time_localtimeMethod(PikaObj* self, Args* args) {
    int64_t secs = args_getInt(args, "secs");
    PikaObj* result = _time_localtime(self, secs);
    if (result) {
        method_returnObj(args, result);
    } else {
        method_returnArg(args, arg_newNone());
    }
}

static void _time_mktimeMethod(PikaObj* self, Args* args) {
    Arg* aT = args_getArg(args, "t");
    if (aT == NULL || !arg_isObject(aT)) {
        method_returnInt(args, 0);
        return;
    }
    PikaObj* t = arg_getPtr(aT);
    method_returnInt(args, _time_mktime(self, t));
}

static void _time_ctimeMethod(PikaObj* self, Args* args) {
    int64_t secs = args_getInt(args, "secs");
    method_returnStr(args, _time_ctime(self, secs));
}

static void _time_asctimeMethod(PikaObj* self, Args* args) {
    method_returnStr(args, _time_asctime(self));
}

/* Constructor */
PikaObj* New__time(Args* args) {
    PikaObj* self = New_TinyObj(args);

    /* Register methods at runtime */
    class_defineMethod(self, "time", "", (Method)_time_timeMethod);
    class_defineMethod(self, "gmtime", "secs", (Method)_time_gmtimeMethod);
    class_defineMethod(self, "localtime", "secs", (Method)_time_localtimeMethod);
    class_defineMethod(self, "mktime", "t", (Method)_time_mktimeMethod);
    class_defineMethod(self, "ctime", "secs", (Method)_time_ctimeMethod);
    class_defineMethod(self, "asctime", "", (Method)_time_asctimeMethod);

    return self;
}
