/*
 * _math_sbf.c - Math module for Solana SBF
 * Uses float functions (sinf, sqrtf, etc.) - pika_float is float on SBF
 */

#include <math.h>
#include "_math.h"

#define PI 3.14159265358979323846f
#define E  2.71828182845904523536f

void _math___init__(PikaObj* self) {
    obj_setFloat(self, "pi", PI);
    obj_setFloat(self, "e", E);
}

pika_float _math_acos(PikaObj* self, pika_float x) {
    return acosf(x);
}

pika_float _math_asin(PikaObj* self, pika_float x) {
    return asinf(x);
}

pika_float _math_atan(PikaObj* self, pika_float x) {
    return atanf(x);
}

pika_float _math_atan2(PikaObj* self, pika_float x, pika_float y) {
    return atan2f(x, y);
}

int _math_ceil(PikaObj* self, pika_float x) {
    return (int)ceilf(x);
}

pika_float _math_cos(PikaObj* self, pika_float x) {
    return cosf(x);
}

pika_float _math_cosh(PikaObj* self, pika_float x) {
    return coshf(x);
}

pika_float _math_degrees(PikaObj* self, pika_float x) {
    return x * 180.0f / PI;
}

pika_float _math_exp(PikaObj* self, pika_float x) {
    return expf(x);
}

pika_float _math_fabs(PikaObj* self, pika_float x) {
    return fabsf(x);
}

int _math_floor(PikaObj* self, pika_float x) {
    return (int)floorf(x);
}

pika_float _math_fmod(PikaObj* self, pika_float x, pika_float y) {
    return fmodf(x, y);
}

pika_float _math_log(PikaObj* self, pika_float x) {
    return logf(x);
}

pika_float _math_log10(PikaObj* self, pika_float x) {
    return log10f(x);
}

pika_float _math_log2(PikaObj* self, pika_float x) {
    return log2f(x);
}

pika_float _math_pow(PikaObj* self, pika_float x, pika_float y) {
    return powf(x, y);
}

pika_float _math_radians(PikaObj* self, pika_float x) {
    return x * PI / 180.0f;
}

pika_float _math_remainder(PikaObj* self, pika_float x, pika_float y) {
    return remainderf(x, y);
}

pika_float _math_sin(PikaObj* self, pika_float x) {
    return sinf(x);
}

pika_float _math_sinh(PikaObj* self, pika_float x) {
    return sinhf(x);
}

pika_float _math_sqrt(PikaObj* self, pika_float x) {
    return sqrtf(x);
}

pika_float _math_tan(PikaObj* self, pika_float x) {
    return tanf(x);
}

pika_float _math_tanh(PikaObj* self, pika_float x) {
    return tanhf(x);
}

pika_float _math_trunc(PikaObj* self, pika_float x) {
    return truncf(x);
}
