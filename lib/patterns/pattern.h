#ifndef PATTERN_H
#define PATTERN_H

#include <stdio.h>

// MACROS
// Calculates precise projected X values of a pixel
#define PROJ_CALC_X(x, y) ((x < 64) ? (0.8660254 * (x - y)) : ((x < 128) ? (111.7173 - 0.8660254 * x) : (109.1192 - 0.8660254 * x)))
// Calculates less precise, but faster projected X values of a pixel
#define PROJ_CALC_INT_X(x, y) ((x < 64) ? ((7 * (x - y)) >> 3) : ((x < 128) ? (112 - ((7 * x) >> 3)) : (109 - ((7 * x) >> 3))))
// Calculates precise projected Y values of a pixel
#define PROJ_CALC_Y(x, y) ((x < 64) ? ((130 - x - y) >> 1) : ((x < 128) ? (y - (x >> 1)) : ((x >> 1) + y - 128)))

#define BLACK 0x0000
#define BLUE 0x001F
#define RED 0xF800
#define GREEN 0x07E0
#define CYAN 0x07FF
#define MAGENTA 0xF81F
#define YELLOW 0xFFE0
#define WHITE 0xFFFF

class Pattern
{
    public:
        virtual void show() = 0;
        virtual void init() = 0;
        inline uint8_t fast_cos(uint16_t x)
        {
            return cos_wave[x % 256];
        };
    private:
        static const uint8_t cos_wave[256];
};

#endif