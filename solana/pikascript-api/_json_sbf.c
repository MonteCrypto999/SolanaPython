/*
 * _json_sbf.c - JSON module for Solana SBF
 * Provides JSON encoding and decoding for basic types
 */

#include "_json.h"
#include "../../src/TinyObj.h"

/* Forward declarations for list/dict constructors */
extern PikaObj* New_PikaStdData_List(Args* args);
extern void PikaStdData_List___init__(PikaObj* self);
extern PikaObj* New_PikaStdData_Dict(Args* args);
extern void PikaStdData_Dict___init__(PikaObj* self);

/* Forward declarations for recursive functions */
static char* json_encode_arg(PikaObj* self, Arg* arg, char* buf, size_t* pos, size_t max);
static Arg* json_parse_value(const char* s, size_t* pos, size_t len);

/* Helper: append string to buffer */
static void buf_append(char* buf, size_t* pos, size_t max, const char* s) {
    while (*s && *pos < max - 1) {
        buf[(*pos)++] = *s++;
    }
}

/* Helper: append char to buffer */
static void buf_appendc(char* buf, size_t* pos, size_t max, char c) {
    if (*pos < max - 1) {
        buf[(*pos)++] = c;
    }
}

/* Helper: append integer to buffer */
static void buf_append_int(char* buf, size_t* pos, size_t max, int64_t n) {
    char tmp[24];
    int i = 0;
    int neg = 0;

    if (n < 0) {
        neg = 1;
        n = -n;
    }

    do {
        tmp[i++] = '0' + (n % 10);
        n /= 10;
    } while (n > 0);

    if (neg) buf_appendc(buf, pos, max, '-');
    while (i > 0) buf_appendc(buf, pos, max, tmp[--i]);
}

/* Helper: append float to buffer */
static void buf_append_float(char* buf, size_t* pos, size_t max, double f) {
    /* Simple float formatting */
    if (f < 0) {
        buf_appendc(buf, pos, max, '-');
        f = -f;
    }

    int64_t int_part = (int64_t)f;
    buf_append_int(buf, pos, max, int_part);

    double frac = f - int_part;
    if (frac > 0.000001) {
        buf_appendc(buf, pos, max, '.');
        for (int d = 0; d < 6 && frac > 0.000001; d++) {
            frac *= 10;
            int digit = (int)frac;
            buf_appendc(buf, pos, max, '0' + digit);
            frac -= digit;
        }
    }
}

/* Helper: encode string with escaping */
static void buf_append_str(char* buf, size_t* pos, size_t max, const char* s) {
    buf_appendc(buf, pos, max, '"');
    while (*s && *pos < max - 2) {
        char c = *s++;
        if (c == '"' || c == '\\') {
            buf_appendc(buf, pos, max, '\\');
        } else if (c == '\n') {
            buf_appendc(buf, pos, max, '\\');
            c = 'n';
        } else if (c == '\r') {
            buf_appendc(buf, pos, max, '\\');
            c = 'r';
        } else if (c == '\t') {
            buf_appendc(buf, pos, max, '\\');
            c = 't';
        }
        buf_appendc(buf, pos, max, c);
    }
    buf_appendc(buf, pos, max, '"');
}

/* Encode a single Arg to JSON */
static char* json_encode_arg(PikaObj* self, Arg* arg, char* buf, size_t* pos, size_t max) {
    if (arg == NULL) {
        buf_append(buf, pos, max, "null");
        return buf;
    }

    ArgType type = arg_getType(arg);

    if (type == ARG_TYPE_NONE) {
        buf_append(buf, pos, max, "null");
    } else if (type == ARG_TYPE_INT) {
        buf_append_int(buf, pos, max, arg_getInt(arg));
    } else if (type == ARG_TYPE_FLOAT) {
        buf_append_float(buf, pos, max, arg_getFloat(arg));
    } else if (type == ARG_TYPE_STRING) {
        buf_append_str(buf, pos, max, arg_getStr(arg));
    } else if (type == ARG_TYPE_BOOL) {
        buf_append(buf, pos, max, arg_getInt(arg) ? "true" : "false");
    } else if (arg_isObject(arg)) {
        PikaObj* obj = arg_getPtr(arg);
        if (obj == NULL) {
            buf_append(buf, pos, max, "null");
        } else {
            /* Check if it's a list */
            int size = pikaList_getSize(obj);
            if (size >= 0) {
                buf_appendc(buf, pos, max, '[');
                for (int i = 0; i < size; i++) {
                    if (i > 0) buf_appendc(buf, pos, max, ',');
                    Arg* item = pikaList_get(obj, i);
                    json_encode_arg(self, item, buf, pos, max);
                }
                buf_appendc(buf, pos, max, ']');
            } else {
                /* Try as dict - just output empty object for now */
                buf_append(buf, pos, max, "{}");
            }
        }
    } else {
        buf_append(buf, pos, max, "null");
    }

    return buf;
}

/* Helper: skip whitespace */
static void skip_ws(const char* s, size_t* pos, size_t len) {
    while (*pos < len && (s[*pos] == ' ' || s[*pos] == '\t' || s[*pos] == '\n' || s[*pos] == '\r')) {
        (*pos)++;
    }
}

/* Helper: parse string */
static Arg* json_parse_string(const char* s, size_t* pos, size_t len) {
    if (s[*pos] != '"') return arg_newNull();
    (*pos)++;

    size_t start = *pos;
    char buf[256];
    size_t buf_pos = 0;

    while (*pos < len && s[*pos] != '"' && buf_pos < 255) {
        if (s[*pos] == '\\' && *pos + 1 < len) {
            (*pos)++;
            char c = s[*pos];
            if (c == 'n') buf[buf_pos++] = '\n';
            else if (c == 'r') buf[buf_pos++] = '\r';
            else if (c == 't') buf[buf_pos++] = '\t';
            else buf[buf_pos++] = c;
        } else {
            buf[buf_pos++] = s[*pos];
        }
        (*pos)++;
    }

    if (*pos < len && s[*pos] == '"') (*pos)++;
    buf[buf_pos] = '\0';

    return arg_newStr(buf);
}

/* Helper: parse number */
static Arg* json_parse_number(const char* s, size_t* pos, size_t len) {
    int neg = 0;
    if (s[*pos] == '-') {
        neg = 1;
        (*pos)++;
    }

    int64_t int_val = 0;
    int is_float = 0;
    double float_val = 0;
    double frac_mult = 0.1;

    while (*pos < len && s[*pos] >= '0' && s[*pos] <= '9') {
        int_val = int_val * 10 + (s[*pos] - '0');
        (*pos)++;
    }

    if (*pos < len && s[*pos] == '.') {
        is_float = 1;
        float_val = (double)int_val;
        (*pos)++;
        while (*pos < len && s[*pos] >= '0' && s[*pos] <= '9') {
            float_val += (s[*pos] - '0') * frac_mult;
            frac_mult *= 0.1;
            (*pos)++;
        }
    }

    if (is_float) {
        return arg_newFloat(neg ? -float_val : float_val);
    } else {
        return arg_newInt(neg ? -int_val : int_val);
    }
}

/* Helper: parse array */
static Arg* json_parse_array(const char* s, size_t* pos, size_t len) {
    if (s[*pos] != '[') return arg_newNull();
    (*pos)++;

    PikaObj* list = newNormalObj(New_PikaStdData_List);
    PikaStdData_List___init__(list);

    skip_ws(s, pos, len);

    if (*pos < len && s[*pos] == ']') {
        (*pos)++;
        return arg_newObj(list);
    }

    while (*pos < len) {
        skip_ws(s, pos, len);
        Arg* item = json_parse_value(s, pos, len);
        pikaList_append(list, item);

        skip_ws(s, pos, len);
        if (*pos >= len) break;
        if (s[*pos] == ']') {
            (*pos)++;
            break;
        }
        if (s[*pos] == ',') (*pos)++;
    }

    return arg_newObj(list);
}

/* Parse JSON value */
static Arg* json_parse_value(const char* s, size_t* pos, size_t len) {
    skip_ws(s, pos, len);
    if (*pos >= len) return arg_newNull();

    char c = s[*pos];

    if (c == '"') {
        return json_parse_string(s, pos, len);
    } else if (c == '[') {
        return json_parse_array(s, pos, len);
    } else if (c == '{') {
        /* Skip objects for now - return empty dict */
        int depth = 1;
        (*pos)++;
        while (*pos < len && depth > 0) {
            if (s[*pos] == '{') depth++;
            else if (s[*pos] == '}') depth--;
            (*pos)++;
        }
        PikaObj* dict = newNormalObj(New_PikaStdData_Dict);
        PikaStdData_Dict___init__(dict);
        return arg_newObj(dict);
    } else if (c == 't' && *pos + 3 < len && s[*pos+1] == 'r' && s[*pos+2] == 'u' && s[*pos+3] == 'e') {
        *pos += 4;
        return arg_newInt(1);
    } else if (c == 'f' && *pos + 4 < len && s[*pos+1] == 'a' && s[*pos+2] == 'l' && s[*pos+3] == 's' && s[*pos+4] == 'e') {
        *pos += 5;
        return arg_newInt(0);
    } else if (c == 'n' && *pos + 3 < len && s[*pos+1] == 'u' && s[*pos+2] == 'l' && s[*pos+3] == 'l') {
        *pos += 4;
        return arg_newNull();
    } else if (c == '-' || (c >= '0' && c <= '9')) {
        return json_parse_number(s, pos, len);
    }

    return arg_newNull();
}

/* Serialize Python object to JSON string */
char* _json_dumps(PikaObj* self, Arg* obj) {
    char* buf = pika_platform_malloc(1024);
    if (buf == NULL) return NULL;

    size_t pos = 0;
    json_encode_arg(self, obj, buf, &pos, 1024);
    buf[pos] = '\0';

    char* result = obj_cacheStr(self, buf);
    pika_platform_free(buf);
    return result;
}

/* Parse JSON string to Python object */
Arg* _json_loads(PikaObj* self, char* s) {
    (void)self;
    if (s == NULL) return arg_newNull();

    /* Calculate string length */
    size_t pos = 0;
    size_t len = 0;
    while (s[len] != '\0') len++;

    return json_parse_value(s, &pos, len);
}

/* Method wrappers */

static void _json_dumpsMethod(PikaObj* self, Args* args) {
    Arg* obj = args_getArg(args, "obj");
    char* result = _json_dumps(self, obj);
    if (result) {
        method_returnStr(args, result);
    } else {
        method_returnStr(args, "null");
    }
}

static void _json_loadsMethod(PikaObj* self, Args* args) {
    char* s = args_getStr(args, "s");
    Arg* result = _json_loads(self, s);
    if (result) {
        method_returnArg(args, result);
    } else {
        method_returnArg(args, arg_newNull());
    }
}

/* Constructor */
PikaObj* New__json(Args* args) {
    PikaObj* self = New_TinyObj(args);

    class_defineMethod(self, "dumps", "obj", (Method)_json_dumpsMethod);
    class_defineMethod(self, "loads", "s", (Method)_json_loadsMethod);

    return self;
}
