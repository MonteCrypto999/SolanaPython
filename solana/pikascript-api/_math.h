/*
 * _math.h - Math module header for Solana SBF
 */

#ifndef ___math__H
#define ___math__H

#include "../../src/PikaObj.h"

PikaObj *New__math(Args *args);

void _math___init__(PikaObj *self);
pika_float _math_acos(PikaObj *self, pika_float x);
pika_float _math_asin(PikaObj *self, pika_float x);
pika_float _math_atan(PikaObj *self, pika_float x);
pika_float _math_atan2(PikaObj *self, pika_float x, pika_float y);
int _math_ceil(PikaObj *self, pika_float x);
pika_float _math_cos(PikaObj *self, pika_float x);
pika_float _math_cosh(PikaObj *self, pika_float x);
pika_float _math_degrees(PikaObj *self, pika_float x);
pika_float _math_exp(PikaObj *self, pika_float x);
pika_float _math_fabs(PikaObj *self, pika_float x);
int _math_floor(PikaObj *self, pika_float x);
pika_float _math_fmod(PikaObj *self, pika_float x, pika_float y);
pika_float _math_log(PikaObj *self, pika_float x);
pika_float _math_log10(PikaObj *self, pika_float x);
pika_float _math_log2(PikaObj *self, pika_float x);
pika_float _math_pow(PikaObj *self, pika_float x, pika_float y);
pika_float _math_radians(PikaObj *self, pika_float x);
pika_float _math_remainder(PikaObj *self, pika_float x, pika_float y);
pika_float _math_sin(PikaObj *self, pika_float x);
pika_float _math_sinh(PikaObj *self, pika_float x);
pika_float _math_sqrt(PikaObj *self, pika_float x);
pika_float _math_tan(PikaObj *self, pika_float x);
pika_float _math_tanh(PikaObj *self, pika_float x);
pika_float _math_trunc(PikaObj *self, pika_float x);

#endif
