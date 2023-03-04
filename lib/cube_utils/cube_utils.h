#ifndef CUBE_UTILS_H
#define CUBE_UTILS_H

#include <stdio.h>
#include <Arduino.h>

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <WebSerial.h>

#include "virtual_displays.h"
#include "config.h"


// MACROS
// Calculates precise projected X values of a pixel
#define PROJ_CALC_X(x, y) ((x < 64) ? (0.8660254 * (x - y)) : ((x < 128) ? (111.7173 - 0.8660254 * x) : (109.1192 - 0.8660254 * x)))
// Calculates less precise, but faster projected X values of a pixel
#define PROJ_CALC_INT_X(x, y) ((x < 64) ? ((7 * (x - y)) >> 3) : ((x < 128) ? (112 - ((7 * x) >> 3)) : (109 - ((7 * x) >> 3))))
// Calculates precise projected Y values of a pixel
#define PROJ_CALC_Y(x, y) ((x < 64) ? ((130 - x - y) >> 1) : ((x < 128) ? (y - (x >> 1)) : ((x >> 1) + y - 128)))

// RGB 565 COLORS
#define BLACK 0x0000
#define BLUE 0x001F
#define RED 0xF800
#define GREEN 0x07E0
#define CYAN 0x07FF
#define MAGENTA 0xF81F
#define YELLOW 0xFFE0
#define WHITE 0xFFFF


// Pattern interface
class Pattern
{
    public:
        virtual void show() = 0;
        virtual void init() = 0;
    protected:
        unsigned long frameCount{0};
        MatrixPanel_I2S_DMA *display;
};

struct PatternServices
{
    MatrixPanel_I2S_DMA *display;
    AsyncWebServer *server;
};

uint8_t fast_cos(uint16_t x);

#endif