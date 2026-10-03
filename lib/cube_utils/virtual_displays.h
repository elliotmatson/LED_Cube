#ifndef VIRTUAL_DISPLAYS_H
#define VIRTUAL_DISPLAYS_H

#include "ESP32-HUB75-MatrixPanel-I2S-DMA.h"
#include "cube_geometry.h"

/**
 * An Adafruit_GFX surface drawn onto part of the HUB75 chain through a fixed
 * coordinate mapping. Subclasses supply map(); everything else -- clipping,
 * fills, fast lines, sprites -- is shared.
 *
 * map() is a pure function of (x, y), so two tasks drawing on the same view
 * no longer race on a shared "current coordinates" member. Drawing is still
 * not synchronized with anything else (the GFX cursor and text state are
 * per-view), so a view should be drawn from one task.
 */
class ChainView : public Adafruit_GFX
{
public:
    MatrixPanel_I2S_DMA *display;
    int16_t virtualResX;
    int16_t virtualResY;

    ChainView(MatrixPanel_I2S_DMA &disp, int16_t w, int16_t h)
        : Adafruit_GFX(w, h), display(&disp), virtualResX(w), virtualResY(h) {}
    virtual ~ChainView() = default;

    /// The chain pixel for view pixel (x, y), or an invalid point when (x, y)
    /// is outside the view. Must be affine (a rotation and translation), which
    /// is what lets fast lines map their two end points.
    virtual cube::Point map(int16_t x, int16_t y) const = 0;

    void drawPixel(int16_t x, int16_t y, uint16_t color) override
    {
        cube::Point p = map(x, y);
        if (p.valid())
        {
            display->drawPixel(p.x, p.y, color);
        }
    }

    void drawPixelRGB888(int16_t x, int16_t y, uint8_t r, uint8_t g, uint8_t b)
    {
        cube::Point p = map(x, y);
        if (p.valid())
        {
            display->drawPixelRGB888(p.x, p.y, r, g, b);
        }
    }

    void fillScreen(uint16_t color) override { fillRect(0, 0, virtualResX, virtualResY, color); }
    void fillScreenRGB888(uint8_t r, uint8_t g, uint8_t b)
    {
        // Row by row in 24-bit colour, rather than through fillScreen's 565.
        for (int16_t y = 0; y < virtualResY; y++)
        {
            drawFastHLine(0, y, virtualResX, r, g, b);
        }
    }
    void clearScreen() { fillScreen(0); }

    uint16_t color444(uint8_t r, uint8_t g, uint8_t b) { return display->color444(r, g, b); }
    uint16_t color565(uint8_t r, uint8_t g, uint8_t b) { return display->color565(r, g, b); }
    void flipDMABuffer() { display->flipDMABuffer(); }

    // Clipped to the view, then drawn as one fast line on the chain. These
    // used to skip clipping, so a line or a GFX fillRect near an edge spilled
    // onto the neighbouring panel.
    void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) override
    {
        uint8_t r, g, b;
        display->color565to888(color, r, g, b);
        drawFastHLine(x, y, w, r, g, b);
    }
    void drawFastHLine(int16_t x, int16_t y, int16_t w, uint8_t r, uint8_t g, uint8_t b)
    {
        int16_t x0 = x, x1 = x + w - 1;
        if (w <= 0 || y < 0 || y >= virtualResY || !clip(x0, x1, virtualResX))
        {
            return;
        }
        drawChainSegment(map(x0, y), map(x1, y), r, g, b);
    }
    void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) override
    {
        uint8_t r, g, b;
        display->color565to888(color, r, g, b);
        drawFastVLine(x, y, h, r, g, b);
    }
    void drawFastVLine(int16_t x, int16_t y, int16_t h, uint8_t r, uint8_t g, uint8_t b)
    {
        int16_t y0 = y, y1 = y + h - 1;
        if (h <= 0 || x < 0 || x >= virtualResX || !clip(y0, y1, virtualResY))
        {
            return;
        }
        drawChainSegment(map(x, y0), map(x, y1), r, g, b);
    }

    /// 1-bit sprites, one row per array element, most significant bit on the
    /// left. `fill` paints the unset bits black. Width at most 8 (or 16).
    void drawSprite8(const uint8_t *sprite, int16_t x, int16_t y, uint16_t w, uint16_t h, uint8_t r, uint8_t g, uint8_t b, bool fill = false)
    {
        drawSprite(sprite, nullptr, w > 8 ? 8 : w, x, y, h, r, g, b, fill);
    }
    void drawSprite8(const uint8_t *sprite, int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t color, bool fill = false)
    {
        uint8_t r, g, b;
        display->color565to888(color, r, g, b);
        drawSprite8(sprite, x, y, w, h, r, g, b, fill);
    }
    void drawSprite16(const uint16_t *sprite, int16_t x, int16_t y, uint16_t w, uint16_t h, uint8_t r, uint8_t g, uint8_t b, bool fill = false)
    {
        drawSprite(nullptr, sprite, w > 16 ? 16 : w, x, y, h, r, g, b, fill);
    }
    void drawSprite16(const uint16_t *sprite, int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t color, bool fill = false)
    {
        uint8_t r, g, b;
        display->color565to888(color, r, g, b);
        drawSprite16(sprite, x, y, w, h, r, g, b, fill);
    }

private:
    static bool clip(int16_t &lo, int16_t &hi, int16_t size)
    {
        if (lo < 0)
            lo = 0;
        if (hi > size - 1)
            hi = size - 1;
        return lo <= hi;
    }

    void drawChainSegment(cube::Point a, cube::Point b, uint8_t r, uint8_t g, uint8_t bl)
    {
        if (!a.valid() || !b.valid())
        {
            return;
        }
        if (a.y == b.y)
        {
            display->drawFastHLine(a.x < b.x ? a.x : b.x, a.y, abs(b.x - a.x) + 1, r, g, bl);
        }
        else
        {
            display->drawFastVLine(a.x, a.y < b.y ? a.y : b.y, abs(b.y - a.y) + 1, r, g, bl);
        }
    }

    void drawSprite(const uint8_t *rows8, const uint16_t *rows16, uint16_t w, int16_t x, int16_t y, uint16_t h,
                    uint8_t r, uint8_t g, uint8_t b, bool fill)
    {
        const int bits = rows8 ? 8 : 16;
        for (int16_t j = 0; j < h; j++)
        {
            const uint16_t row = rows8 ? rows8[j] : rows16[j];
            for (int16_t i = 0; i < w; i++)
            {
                if ((row >> (bits - 1 - i)) & 1)
                {
                    drawPixelRGB888(x + i, y + j, r, g, b);
                }
                else if (fill)
                {
                    drawPixelRGB888(x + i, y + j, 0, 0, 0);
                }
            }
        }
    }
};

/// One 64x64 face, rotated in quarter turns (see cube::faceToChain).
class SinglePanel : public ChainView
{
public:
    SinglePanel(MatrixPanel_I2S_DMA &disp, int panel, int rotate)
        : ChainView(disp, cube::FACE_SIZE, cube::FACE_SIZE), _panel(panel), _rotate(rotate & 3) {}

    /// @param rotate 0 = none, 1 = 90 degrees, 2 = 180, 3 = 270. Does not
    /// redraw what is already on the face.
    void setRotation(int rotate) { _rotate = rotate & 3; }

    cube::Point map(int16_t x, int16_t y) const override { return cube::faceToChain(_panel, _rotate, x, y); }

private:
    int _panel;
    int _rotate;
};

/// A 128x64 strip across the two side faces (physical faces 2 then 1),
/// turned 180 degrees so it reads upright: x = 0 is the far edge of face 2,
/// and the seam between the faces is between x = 63 and 64.
class BottomPanels : public ChainView
{
public:
    explicit BottomPanels(MatrixPanel_I2S_DMA &disp)
        : ChainView(disp, 2 * cube::FACE_SIZE, cube::FACE_SIZE) {}

    cube::Point map(int16_t x, int16_t y) const override
    {
        if (x < 0 || x >= virtualResX || y < 0 || y >= virtualResY)
        {
            return cube::NO_POINT;
        }
        return cube::Point{int16_t(cube::CHAIN_WIDTH - 1 - x), int16_t(cube::CHAIN_HEIGHT - 1 - y)};
    }
};

#endif
