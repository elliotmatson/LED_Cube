#ifndef CUBE_UTILS_H
#define CUBE_UTILS_H

#include <stdio.h>
#include <Arduino.h>

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

#include <ESPAsyncWebServer.h>

#include "virtual_displays.h"
#include "config.h"

// Seam adjacency, face mappings, the 3D surface mapping and the isometric
// projection (formerly the PROJ_CALC_* macros) live in lib/cube_geometry,
// where they are unit tested.
#include "cube_geometry.h"

// RGB 565 COLORS
#define BLACK 0x0000
#define BLUE 0x001F
#define RED 0xF800
#define GREEN 0x07E0
#define CYAN 0x07FF
#define MAGENTA 0xF81F
#define YELLOW 0xFFE0
#define WHITE 0xFFFF

struct PatternServices
{
    MatrixPanel_I2S_DMA *display;
    AsyncWebServer *server;
};

struct PatternData
{
    std::string name;
};

// Pattern interface
class Pattern
{
public:
    virtual void init(PatternServices *pattern) = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
    std::string getName() { return data.name; };

protected:
    // Null while the pattern is not running. stop() implementations must
    // check it and clear it: vTaskDelete(NULL) deletes the *calling* task, and
    // a stale handle may already belong to a different task.
    TaskHandle_t refreshTask{nullptr};
    unsigned long frameCount{0};
    PatternServices *pattern{nullptr};
    PatternData data;
};

uint8_t fast_cos(uint16_t x);

#endif