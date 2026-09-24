// Included before every game source file. The game called libultra's sinf and
// cosf, whose results differ from the host's libm; the SDK's own versions are
// compiled under other names so the rest of the process keeps libm's.
#ifndef ULTRA_MATH_H
#define ULTRA_MATH_H

#include <math.h>

float ultra_sinf(float x);
float ultra_cosf(float x);
#define sinf ultra_sinf
#define cosf ultra_cosf

#endif
