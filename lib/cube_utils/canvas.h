#ifndef CANVAS_H
#define CANVAS_H

#include <Adafruit_GFX.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

#include "cube_geometry.h"

/**
 * The 192x64 frame patterns draw into: RGB888 in PSRAM, with the drawing
 * calls patterns used to make on MatrixPanel_I2S_DMA directly.
 *
 * Only the render task draws on it and pushes it to the panels (push()), so
 * the HUB75 DMA buffer has a single writer. A frame reaches the panels whole:
 * a pattern that clears and redraws (Clock) no longer shows the cleared state
 * for the length of a refresh.
 */
class Canvas : public Adafruit_GFX
{
public:
    Canvas() : Adafruit_GFX(cube::CHAIN_WIDTH, cube::CHAIN_HEIGHT) {}
    ~Canvas();

    /// Allocates the frame. Returns false if PSRAM is exhausted.
    bool begin();

    void drawPixel(int16_t x, int16_t y, uint16_t color) override;
    void drawPixelRGB888(int16_t x, int16_t y, uint8_t r, uint8_t g, uint8_t b);
    void fillScreen(uint16_t color) override;
    void fillScreenRGB888(uint8_t r, uint8_t g, uint8_t b);
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override;
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t r, uint8_t g, uint8_t b);
    void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) override { fillRect(x, y, w, 1, color); }
    void drawFastHLine(int16_t x, int16_t y, int16_t w, uint8_t r, uint8_t g, uint8_t b) { fillRect(x, y, w, 1, r, g, b); }
    void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) override { fillRect(x, y, 1, h, color); }
    void drawFastVLine(int16_t x, int16_t y, int16_t h, uint8_t r, uint8_t g, uint8_t b) { fillRect(x, y, 1, h, r, g, b); }

    /**
     * Direct access to `count` pixels of row y starting at x, as packed
     * RGB888, for patterns that compute whole rows: no per-pixel call, bounds
     * check or dirty-span update. The span is marked changed up front.
     *
     * @return nullptr if the span is not entirely on the canvas.
     */
    uint8_t *rowForWrite(int16_t y, int16_t x, int16_t count)
    {
        if (pixels == nullptr || y < 0 || y >= cube::CHAIN_HEIGHT || x < 0 || count <= 0 || x + count > cube::CHAIN_WIDTH)
        {
            return nullptr;
        }
        markDirty(y, x, x + count - 1);
        return at(x, y);
    }

    static uint16_t color565(uint8_t r, uint8_t g, uint8_t b) { return MatrixPanel_I2S_DMA::color565(r, g, b); }
    static uint16_t color444(uint8_t r, uint8_t g, uint8_t b) { return MatrixPanel_I2S_DMA::color444(r, g, b); }
    static void color565to888(uint16_t color, uint8_t &r, uint8_t &g, uint8_t &b) { MatrixPanel_I2S_DMA::color565to888(color, r, g, b); }

    /// Copies what changed since the last push to the panels: for each row,
    /// the span from its leftmost to its rightmost drawn pixel. Pixels redrawn
    /// with the same value count as changed.
    void push(MatrixPanel_I2S_DMA &panels);

    /// Makes the next push copy everything, e.g. after something drew on the
    /// panels directly (boot messages, update progress).
    void markAllDirty();

private:
    uint8_t *pixels = nullptr;
    // Per row, the changed span [dirtyFrom, dirtyTo]; empty when from > to.
    // Pushing costs time per pixel (the HUB75 library's bit-plane update),
    // so a pattern that draws one face, or one line, should not pay for all
    // 192 columns.
    int16_t dirtyFrom[cube::CHAIN_HEIGHT];
    int16_t dirtyTo[cube::CHAIN_HEIGHT];

    void markDirty(int16_t y, int16_t x0, int16_t x1)
    {
        if (x0 < dirtyFrom[y])
            dirtyFrom[y] = x0;
        if (x1 > dirtyTo[y])
            dirtyTo[y] = x1;
    }

    uint8_t *at(int16_t x, int16_t y) { return pixels + (size_t(y) * cube::CHAIN_WIDTH + x) * 3; }
};

#endif
