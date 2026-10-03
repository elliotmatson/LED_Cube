#ifndef CUBE_UTILS_H
#define CUBE_UTILS_H

#include <stdio.h>
#include <Arduino.h>

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

#include <ESPAsyncWebServer.h>

#include "canvas.h"
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
    // What patterns draw on. Pushed to the panels by the render task after
    // every tick().
    Canvas *display;
    AsyncWebServer *server;
};

struct PatternData
{
    std::string id;   // stable, saved in settings; never shown
    std::string name; // shown on the dashboard
};

/**
 * A pattern, driven by the cube's single render task:
 *
 *   begin() once when selected, then tick() every frameInterval() ms, then
 *   end() when another pattern is selected or an update starts. begin() may
 *   be called again after end().
 *
 * All three run in the render task, so a pattern needs no locking between
 * them and never creates or deletes the task that draws it. Patterns that do
 * slow I/O (Spotify) start a worker in begin(), hand results to tick() under
 * a lock, and stop the worker cooperatively in end() -- tick() must not block.
 */
class Pattern
{
public:
    virtual ~Pattern() = default;
    virtual void begin(PatternServices *services) = 0;
    virtual void tick() = 0;
    virtual void end() {}
    /// Milliseconds between ticks. A tick that overruns is followed by the
    /// next one straight away (after a one-tick yield), not by catching up.
    virtual uint32_t frameInterval() const { return 33; }
    std::string getName() { return data.name; };
    std::string getId() { return data.id; };

protected:
    unsigned long frameCount{0};
    PatternServices *pattern{nullptr};
    PatternData data;
};

extern const uint8_t cos_wave[256];

/// (1 - cos) / 2 scaled to 0..255, one period per 256 steps. Inline: Plasma
/// calls it six times a pixel.
inline uint8_t fast_cos(uint16_t x) { return cos_wave[x & 0xFF]; }

#endif