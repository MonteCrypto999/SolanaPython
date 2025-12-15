/*
 * _base64_sbf.c - Base64 module for Solana SBF
 * Provides base64 encoding and decoding
 */

#include "_base64.h"
#include "../../src/TinyObj.h"

/* Base64 encoding table */
static const char b64_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* Base64 decoding table (ASCII to 6-bit value, 255 = invalid) */
static const uint8_t b64_decode_table[128] = {
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255, 62,255,255,255, 63,
     52, 53, 54, 55, 56, 57, 58, 59, 60, 61,255,255,255,  0,255,255,
    255,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
     15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,255,255,255,255,255,
    255, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
     41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51,255,255,255,255,255
};

/* Encode bytes to base64 string */
char* _base64_b64encode(PikaObj* self, Arg* data) {
    if (data == NULL) return NULL;

    uint8_t* data_ptr = NULL;
    size_t data_len = 0;

    if (!get_bytes_from_arg(data, &data_ptr, &data_len)) return NULL;

    /* Calculate output length: 4 chars for every 3 bytes, rounded up */
    size_t out_len = ((data_len + 2) / 3) * 4;

    /* Allocate buffer (add 1 for null terminator) */
    char* out = pika_platform_malloc(out_len + 1);
    if (out == NULL) return NULL;

    size_t i = 0, j = 0;
    while (i < data_len) {
        uint32_t octet_a = i < data_len ? data_ptr[i++] : 0;
        uint32_t octet_b = i < data_len ? data_ptr[i++] : 0;
        uint32_t octet_c = i < data_len ? data_ptr[i++] : 0;

        uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;

        out[j++] = b64_table[(triple >> 18) & 0x3F];
        out[j++] = b64_table[(triple >> 12) & 0x3F];
        out[j++] = b64_table[(triple >> 6) & 0x3F];
        out[j++] = b64_table[triple & 0x3F];
    }

    /* Add padding */
    size_t mod = data_len % 3;
    if (mod == 1) {
        out[j - 2] = '=';
        out[j - 1] = '=';
    } else if (mod == 2) {
        out[j - 1] = '=';
    }

    out[j] = '\0';

    /* Cache string in object for memory management */
    char* result = obj_cacheStr(self, out);
    pika_platform_free(out);
    return result;
}

/* Decode base64 string to bytes */
Arg* _base64_b64decode(PikaObj* self, char* s) {
    (void)self;
    if (s == NULL) return arg_newNull();

    /* Calculate string length */
    size_t in_len = 0;
    while (s[in_len] != '\0') in_len++;
    if (in_len == 0) return arg_newBytes(NULL, 0);

    /* Remove padding from length calculation */
    size_t padding = 0;
    if (in_len > 0 && s[in_len - 1] == '=') padding++;
    if (in_len > 1 && s[in_len - 2] == '=') padding++;

    /* Calculate output length */
    size_t out_len = (in_len / 4) * 3 - padding;

    uint8_t* out = pika_platform_malloc(out_len);
    if (out == NULL) return arg_newNull();

    size_t i = 0, j = 0;
    while (i < in_len) {
        uint8_t a = b64_decode_table[(uint8_t)s[i]]; i++;
        uint8_t b = (i < in_len) ? b64_decode_table[(uint8_t)s[i]] : 255; i++;
        uint8_t c = (i < in_len && s[i] != '=') ? b64_decode_table[(uint8_t)s[i]] : 0; i++;
        uint8_t d = (i < in_len && s[i] != '=') ? b64_decode_table[(uint8_t)s[i]] : 0; i++;

        if (a == 255 || b == 255) {
            pika_platform_free(out);
            return arg_newNull();  /* Invalid input */
        }

        uint32_t triple = (a << 18) | (b << 12) | (c << 6) | d;

        if (j < out_len) out[j++] = (triple >> 16) & 0xFF;
        if (j < out_len) out[j++] = (triple >> 8) & 0xFF;
        if (j < out_len) out[j++] = triple & 0xFF;
    }

    Arg* result = arg_newBytes(out, out_len);
    pika_platform_free(out);
    return result;
}

/* Method wrappers */

static void _base64_b64encodeMethod(PikaObj* self, Args* args) {
    Arg* data = args_getArg(args, "data");
    char* result = _base64_b64encode(self, data);
    if (result) {
        method_returnStr(args, result);
    } else {
        method_returnStr(args, "");
    }
}

static void _base64_b64decodeMethod(PikaObj* self, Args* args) {
    char* s = args_getStr(args, "s");
    Arg* result = _base64_b64decode(self, s);
    if (result) {
        method_returnArg(args, result);
    } else {
        method_returnArg(args, arg_newNull());
    }
}

/* Constructor */
PikaObj* New__base64(Args* args) {
    PikaObj* self = New_TinyObj(args);

    class_defineMethod(self, "b64encode", "data", (Method)_base64_b64encodeMethod);
    class_defineMethod(self, "b64decode", "s", (Method)_base64_b64decodeMethod);

    return self;
}
