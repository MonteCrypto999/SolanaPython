/*
 * base58 module implementation for Solana SBF
 *
 * Provides base58 encoding/decoding functions commonly used in Solana
 * for pubkeys, signatures, and other binary data.
 *
 * Uses the base58 functions from solana_vfs.c
 */

#include "_base58.h"
#include "../../src/TinyObj.h"

/* Use the existing base58 functions from solana_vfs.c */
extern int sol_b58_decode(const char* b58, uint8_t* out);
extern int sol_b58_encode(const uint8_t* data, size_t len, char* out);

/* Helper to get bytes from arg (defined in pika_python.c) */
extern int get_bytes_from_arg(Arg* arg, uint8_t** out_ptr, size_t* out_len);

/*
 * b58decode(s) -> bytes
 * Decode a base58 string to bytes
 */
Arg* _base58_b58decode(PikaObj* self, char* s) {
    (void)self;
    if (s == NULL) return arg_newNull();

    /* Decode to temporary buffer (max 64 bytes for most Solana types) */
    uint8_t buf[64];
    int len = sol_b58_decode(s, buf);
    if (len < 0) return arg_newNull();

    return arg_newBytes(buf, len);
}

/*
 * b58encode(data) -> str
 * Encode bytes to a base58 string
 *
 * Note: Returns heap-allocated string. The caller (PikaPython) will
 * copy it and the string is freed after the method returns.
 */
char* _base58_b58encode(PikaObj* self, Arg* data) {
    (void)self;
    if (data == NULL) return "";

    uint8_t* data_ptr = NULL;
    size_t data_len = 0;

    if (!get_bytes_from_arg(data, &data_ptr, &data_len)) {
        return "";
    }

    /* Encode to heap-allocated string (SBF doesn't support static/BSS) */
    char* buf = (char*)pika_platform_malloc(128);
    if (buf == NULL) return "";

    int len = sol_b58_encode(data_ptr, data_len, buf);
    if (len < 0) {
        pika_platform_free(buf);
        return "";
    }

    /* Note: The string needs to be returned, but PikaPython will copy it.
     * We'll use obj_setStr to store it temporarily and return that. */
    obj_setStr(self, "_tmp_b58", buf);
    pika_platform_free(buf);
    return obj_getStr(self, "_tmp_b58");
}

/* Method wrappers for PikaPython binding */
static void _base58_b58decodeMethod(PikaObj* self, Args* args) {
    char* s = args_getStr(args, "s");
    Arg* result = _base58_b58decode(self, s);
    method_returnArg(args, result);
}

static void _base58_b58encodeMethod(PikaObj* self, Args* args) {
    Arg* data = args_getArg(args, "data");
    char* result = _base58_b58encode(self, data);
    method_returnStr(args, result);
}

/* Module constructor */
PikaObj* New__base58(Args* args) {
    (void)args;
    PikaObj* self = New_TinyObj(NULL);
    if (self == NULL) return NULL;

    self->refcnt = 1;
    obj_setFlag(self, OBJ_FLAG_ALREADY_INIT);

    /* Register methods */
    class_defineMethod(self, "b58decode", "s", (Method)_base58_b58decodeMethod);
    class_defineMethod(self, "b58encode", "data", (Method)_base58_b58encodeMethod);

    return self;
}
