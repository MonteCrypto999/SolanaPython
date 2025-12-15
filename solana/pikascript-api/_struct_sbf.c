/*
 * _struct_sbf.c - Struct module for Solana SBF
 * Binary packing/unpacking (subset of Python struct module)
 *
 * Format codes:
 *   < = little-endian (default), > = big-endian
 *   b = int8, B = uint8
 *   h = int16, H = uint16
 *   i = int32, I = uint32
 *   q = int64, Q = uint64
 *   f = float32
 */

#include "_struct.h"
#include "../../src/TinyObj.h"

/* Forward declaration for tuple constructor */
extern PikaObj* New_PikaStdData_Tuple(Args* args);

/* Calculate size of packed data for format string */
int _struct_calcsize(PikaObj* self, char* fmt) {
    (void)self;
    if (fmt == NULL) return 0;

    int size = 0;
    const char* p = fmt;

    /* Skip endianness marker */
    if (*p == '<' || *p == '>' || *p == '!' || *p == '=') p++;

    while (*p) {
        int count = 0;
        /* Parse repeat count */
        while (*p >= '0' && *p <= '9') {
            count = count * 10 + (*p - '0');
            p++;
        }
        if (count == 0) count = 1;

        switch (*p) {
            case 'b': case 'B': case 'c': case 'x':
                size += count * 1;
                break;
            case 'h': case 'H':
                size += count * 2;
                break;
            case 'i': case 'I': case 'l': case 'L': case 'f':
                size += count * 4;
                break;
            case 'q': case 'Q': case 'd':
                size += count * 8;
                break;
            default:
                break;
        }
        if (*p) p++;
    }

    return size;
}

/* Pack values into bytes according to format string */
Arg* _struct_pack(PikaObj* self, char* fmt, PikaObj* values) {
    (void)self;
    if (fmt == NULL || values == NULL) return arg_newNull();

    int size = _struct_calcsize(self, fmt);
    if (size <= 0) return arg_newNull();

    uint8_t* buf = pika_platform_malloc(size);
    if (buf == NULL) return arg_newNull();
    pika_platform_memset(buf, 0, size);

    const char* p = fmt;
    int little_endian = 1;  /* Default to little-endian */

    /* Check endianness marker */
    if (*p == '<') { little_endian = 1; p++; }
    else if (*p == '>' || *p == '!') { little_endian = 0; p++; }
    else if (*p == '=') { little_endian = 1; p++; }  /* Native = little on most */

    int val_idx = 0;
    int buf_pos = 0;

    while (*p && buf_pos < size) {
        int count = 0;
        while (*p >= '0' && *p <= '9') {
            count = count * 10 + (*p - '0');
            p++;
        }
        if (count == 0) count = 1;

        char code = *p;
        if (*p) p++;

        for (int i = 0; i < count && buf_pos < size; i++) {
            int64_t val = pikaList_getInt(values, val_idx++);

            switch (code) {
                case 'b': case 'B':
                    buf[buf_pos++] = (uint8_t)(val & 0xFF);
                    break;
                case 'h': case 'H':
                    if (little_endian) {
                        buf[buf_pos++] = (uint8_t)(val & 0xFF);
                        buf[buf_pos++] = (uint8_t)((val >> 8) & 0xFF);
                    } else {
                        buf[buf_pos++] = (uint8_t)((val >> 8) & 0xFF);
                        buf[buf_pos++] = (uint8_t)(val & 0xFF);
                    }
                    break;
                case 'i': case 'I': case 'l': case 'L':
                    if (little_endian) {
                        buf[buf_pos++] = (uint8_t)(val & 0xFF);
                        buf[buf_pos++] = (uint8_t)((val >> 8) & 0xFF);
                        buf[buf_pos++] = (uint8_t)((val >> 16) & 0xFF);
                        buf[buf_pos++] = (uint8_t)((val >> 24) & 0xFF);
                    } else {
                        buf[buf_pos++] = (uint8_t)((val >> 24) & 0xFF);
                        buf[buf_pos++] = (uint8_t)((val >> 16) & 0xFF);
                        buf[buf_pos++] = (uint8_t)((val >> 8) & 0xFF);
                        buf[buf_pos++] = (uint8_t)(val & 0xFF);
                    }
                    break;
                case 'q': case 'Q':
                    if (little_endian) {
                        for (int j = 0; j < 8; j++) {
                            buf[buf_pos++] = (uint8_t)((val >> (j * 8)) & 0xFF);
                        }
                    } else {
                        for (int j = 7; j >= 0; j--) {
                            buf[buf_pos++] = (uint8_t)((val >> (j * 8)) & 0xFF);
                        }
                    }
                    break;
                case 'x':
                    buf[buf_pos++] = 0;  /* Padding byte */
                    val_idx--;  /* Don't consume a value */
                    break;
                default:
                    break;
            }
        }
    }

    Arg* result = arg_newBytes(buf, size);
    pika_platform_free(buf);
    return result;
}

/* Unpack bytes into tuple according to format string */
PikaObj* _struct_unpack(PikaObj* self, char* fmt, Arg* data) {
    (void)self;
    if (fmt == NULL || data == NULL) return NULL;

    uint8_t* buf = NULL;
    size_t buf_len = 0;

    if (!get_bytes_from_arg(data, &buf, &buf_len)) return NULL;

    PikaObj* tuple = newNormalObj(New_PikaStdData_Tuple);
    pikaList_init(tuple);  /* Initialize as list for storage */

    const char* p = fmt;
    int little_endian = 1;

    if (*p == '<') { little_endian = 1; p++; }
    else if (*p == '>' || *p == '!') { little_endian = 0; p++; }
    else if (*p == '=') { little_endian = 1; p++; }

    size_t buf_pos = 0;

    while (*p && buf_pos < buf_len) {
        int count = 0;
        while (*p >= '0' && *p <= '9') {
            count = count * 10 + (*p - '0');
            p++;
        }
        if (count == 0) count = 1;

        char code = *p;
        if (*p) p++;

        for (int i = 0; i < count && buf_pos < buf_len; i++) {
            int64_t val = 0;

            switch (code) {
                case 'b':
                    val = (int8_t)buf[buf_pos++];
                    pikaList_append(tuple, arg_newInt(val));
                    break;
                case 'B':
                    val = buf[buf_pos++];
                    pikaList_append(tuple, arg_newInt(val));
                    break;
                case 'h':
                    if (little_endian) {
                        val = buf[buf_pos] | (buf[buf_pos + 1] << 8);
                    } else {
                        val = (buf[buf_pos] << 8) | buf[buf_pos + 1];
                    }
                    val = (int16_t)val;
                    buf_pos += 2;
                    pikaList_append(tuple, arg_newInt(val));
                    break;
                case 'H':
                    if (little_endian) {
                        val = buf[buf_pos] | (buf[buf_pos + 1] << 8);
                    } else {
                        val = (buf[buf_pos] << 8) | buf[buf_pos + 1];
                    }
                    buf_pos += 2;
                    pikaList_append(tuple, arg_newInt(val));
                    break;
                case 'i': case 'l':
                    if (little_endian) {
                        val = buf[buf_pos] | (buf[buf_pos + 1] << 8) |
                              (buf[buf_pos + 2] << 16) | (buf[buf_pos + 3] << 24);
                    } else {
                        val = (buf[buf_pos] << 24) | (buf[buf_pos + 1] << 16) |
                              (buf[buf_pos + 2] << 8) | buf[buf_pos + 3];
                    }
                    val = (int32_t)val;
                    buf_pos += 4;
                    pikaList_append(tuple, arg_newInt(val));
                    break;
                case 'I': case 'L':
                    if (little_endian) {
                        val = buf[buf_pos] | (buf[buf_pos + 1] << 8) |
                              (buf[buf_pos + 2] << 16) | ((uint32_t)buf[buf_pos + 3] << 24);
                    } else {
                        val = ((uint32_t)buf[buf_pos] << 24) | (buf[buf_pos + 1] << 16) |
                              (buf[buf_pos + 2] << 8) | buf[buf_pos + 3];
                    }
                    buf_pos += 4;
                    pikaList_append(tuple, arg_newInt(val));
                    break;
                case 'q': case 'Q':
                    val = 0;
                    if (little_endian) {
                        for (int j = 0; j < 8; j++) {
                            val |= ((uint64_t)buf[buf_pos + j] << (j * 8));
                        }
                    } else {
                        for (int j = 0; j < 8; j++) {
                            val |= ((uint64_t)buf[buf_pos + j] << ((7 - j) * 8));
                        }
                    }
                    buf_pos += 8;
                    pikaList_append(tuple, arg_newInt(val));
                    break;
                case 'x':
                    buf_pos++;  /* Skip padding byte */
                    break;
                default:
                    break;
            }
        }
    }

    return tuple;
}

/* Method wrappers */

static void _struct_packMethod(PikaObj* self, Args* args) {
    char* fmt = args_getStr(args, "fmt");
    Arg* aValues = args_getArg(args, "values");
    PikaObj* values = NULL;
    if (aValues != NULL && arg_isObject(aValues)) {
        values = arg_getPtr(aValues);
    }

    Arg* result = _struct_pack(self, fmt, values);
    if (result) {
        method_returnArg(args, result);
    } else {
        method_returnArg(args, arg_newNull());
    }
}

static void _struct_unpackMethod(PikaObj* self, Args* args) {
    char* fmt = args_getStr(args, "fmt");
    Arg* data = args_getArg(args, "data");

    PikaObj* result = _struct_unpack(self, fmt, data);
    if (result) {
        method_returnObj(args, result);
    } else {
        method_returnArg(args, arg_newNull());
    }
}

static void _struct_calcsizeMethod(PikaObj* self, Args* args) {
    char* fmt = args_getStr(args, "fmt");
    method_returnInt(args, _struct_calcsize(self, fmt));
}

/* Constructor */
PikaObj* New__struct(Args* args) {
    PikaObj* self = New_TinyObj(args);

    class_defineMethod(self, "pack", "fmt,values", (Method)_struct_packMethod);
    class_defineMethod(self, "unpack", "fmt,data", (Method)_struct_unpackMethod);
    class_defineMethod(self, "calcsize", "fmt", (Method)_struct_calcsizeMethod);

    return self;
}
