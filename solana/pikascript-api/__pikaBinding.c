/*
 * Math module bindings for Solana SBF
 * Uses runtime method registration (required for SBF - const array relocations don't work)
 */

#include "_math.h"
#include "../../src/TinyObj.h"

/* Method wrappers */

static void _math_sinMethod(PikaObj *self, Args *args) {
    pika_float x = args_getFloat(args, "x");
    method_returnFloat(args, _math_sin(self, x));
}

static void _math_cosMethod(PikaObj *self, Args *args) {
    pika_float x = args_getFloat(args, "x");
    method_returnFloat(args, _math_cos(self, x));
}

static void _math_tanMethod(PikaObj *self, Args *args) {
    pika_float x = args_getFloat(args, "x");
    method_returnFloat(args, _math_tan(self, x));
}

static void _math_asinMethod(PikaObj *self, Args *args) {
    pika_float x = args_getFloat(args, "x");
    method_returnFloat(args, _math_asin(self, x));
}

static void _math_acosMethod(PikaObj *self, Args *args) {
    pika_float x = args_getFloat(args, "x");
    method_returnFloat(args, _math_acos(self, x));
}

static void _math_atanMethod(PikaObj *self, Args *args) {
    pika_float x = args_getFloat(args, "x");
    method_returnFloat(args, _math_atan(self, x));
}

static void _math_atan2Method(PikaObj *self, Args *args) {
    pika_float x = args_getFloat(args, "x");
    pika_float y = args_getFloat(args, "y");
    method_returnFloat(args, _math_atan2(self, x, y));
}

static void _math_sinhMethod(PikaObj *self, Args *args) {
    pika_float x = args_getFloat(args, "x");
    method_returnFloat(args, _math_sinh(self, x));
}

static void _math_coshMethod(PikaObj *self, Args *args) {
    pika_float x = args_getFloat(args, "x");
    method_returnFloat(args, _math_cosh(self, x));
}

static void _math_tanhMethod(PikaObj *self, Args *args) {
    pika_float x = args_getFloat(args, "x");
    method_returnFloat(args, _math_tanh(self, x));
}

static void _math_sqrtMethod(PikaObj *self, Args *args) {
    pika_float x = args_getFloat(args, "x");
    method_returnFloat(args, _math_sqrt(self, x));
}

static void _math_floorMethod(PikaObj *self, Args *args) {
    pika_float x = args_getFloat(args, "x");
    method_returnInt(args, _math_floor(self, x));
}

static void _math_ceilMethod(PikaObj *self, Args *args) {
    pika_float x = args_getFloat(args, "x");
    method_returnInt(args, _math_ceil(self, x));
}

static void _math_fabsMethod(PikaObj *self, Args *args) {
    pika_float x = args_getFloat(args, "x");
    method_returnFloat(args, _math_fabs(self, x));
}

static void _math_powMethod(PikaObj *self, Args *args) {
    pika_float x = args_getFloat(args, "x");
    pika_float y = args_getFloat(args, "y");
    method_returnFloat(args, _math_pow(self, x, y));
}

static void _math_logMethod(PikaObj *self, Args *args) {
    pika_float x = args_getFloat(args, "x");
    method_returnFloat(args, _math_log(self, x));
}

static void _math_log10Method(PikaObj *self, Args *args) {
    pika_float x = args_getFloat(args, "x");
    method_returnFloat(args, _math_log10(self, x));
}

static void _math_log2Method(PikaObj *self, Args *args) {
    pika_float x = args_getFloat(args, "x");
    method_returnFloat(args, _math_log2(self, x));
}

static void _math_expMethod(PikaObj *self, Args *args) {
    pika_float x = args_getFloat(args, "x");
    method_returnFloat(args, _math_exp(self, x));
}

static void _math_degreesMethod(PikaObj *self, Args *args) {
    pika_float x = args_getFloat(args, "x");
    method_returnFloat(args, _math_degrees(self, x));
}

static void _math_radiansMethod(PikaObj *self, Args *args) {
    pika_float x = args_getFloat(args, "x");
    method_returnFloat(args, _math_radians(self, x));
}

static void _math_fmodMethod(PikaObj *self, Args *args) {
    pika_float x = args_getFloat(args, "x");
    pika_float y = args_getFloat(args, "y");
    method_returnFloat(args, _math_fmod(self, x, y));
}

static void _math_truncMethod(PikaObj *self, Args *args) {
    pika_float x = args_getFloat(args, "x");
    method_returnFloat(args, _math_trunc(self, x));
}

static void _math_remainderMethod(PikaObj *self, Args *args) {
    pika_float x = args_getFloat(args, "x");
    pika_float y = args_getFloat(args, "y");
    method_returnFloat(args, _math_remainder(self, x, y));
}

/* Constructor */
PikaObj *New__math(Args *args) {
    PikaObj *self = New_TinyObj(args);

    /* Set pi and e constants */
    _math___init__(self);

    /* Register methods at runtime */
    class_defineMethod(self, "sin", "x", (Method)_math_sinMethod);
    class_defineMethod(self, "cos", "x", (Method)_math_cosMethod);
    class_defineMethod(self, "tan", "x", (Method)_math_tanMethod);
    class_defineMethod(self, "asin", "x", (Method)_math_asinMethod);
    class_defineMethod(self, "acos", "x", (Method)_math_acosMethod);
    class_defineMethod(self, "atan", "x", (Method)_math_atanMethod);
    class_defineMethod(self, "atan2", "x,y", (Method)_math_atan2Method);
    class_defineMethod(self, "sinh", "x", (Method)_math_sinhMethod);
    class_defineMethod(self, "cosh", "x", (Method)_math_coshMethod);
    class_defineMethod(self, "tanh", "x", (Method)_math_tanhMethod);
    class_defineMethod(self, "sqrt", "x", (Method)_math_sqrtMethod);
    class_defineMethod(self, "floor", "x", (Method)_math_floorMethod);
    class_defineMethod(self, "ceil", "x", (Method)_math_ceilMethod);
    class_defineMethod(self, "fabs", "x", (Method)_math_fabsMethod);
    class_defineMethod(self, "pow", "x,y", (Method)_math_powMethod);
    class_defineMethod(self, "log", "x", (Method)_math_logMethod);
    class_defineMethod(self, "log10", "x", (Method)_math_log10Method);
    class_defineMethod(self, "log2", "x", (Method)_math_log2Method);
    class_defineMethod(self, "exp", "x", (Method)_math_expMethod);
    class_defineMethod(self, "degrees", "x", (Method)_math_degreesMethod);
    class_defineMethod(self, "radians", "x", (Method)_math_radiansMethod);
    class_defineMethod(self, "fmod", "x,y", (Method)_math_fmodMethod);
    class_defineMethod(self, "trunc", "x", (Method)_math_truncMethod);
    class_defineMethod(self, "remainder", "x,y", (Method)_math_remainderMethod);

    return self;
}
