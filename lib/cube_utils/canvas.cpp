#include "canvas.h"

#include <esp_heap_caps.h>
#include <string.h>

Canvas::~Canvas()
{
    heap_caps_free(pixels);
}

bool Canvas::begin()
{
    if (pixels == nullptr)
    {
        // 36 KB. PSRAM: internal RAM is what WiFi, TLS and the HUB75 DMA
        // buffers compete for.
        pixels = static_cast<uint8_t *>(heap_caps_calloc(size_t(cube::CHAIN_WIDTH) * cube::CHAIN_HEIGHT, 3, MALLOC_CAP_SPIRAM));
    }
    markAllDirty();
    return pixels != nullptr;
}

void Canvas::drawPixelRGB888(int16_t x, int16_t y, uint8_t r, uint8_t g, uint8_t b)
{
    if (pixels == nullptr || x < 0 || y < 0 || x >= cube::CHAIN_WIDTH || y >= cube::CHAIN_HEIGHT)
    {
        return;
    }
    uint8_t *p = at(x, y);
    p[0] = r;
    p[1] = g;
    p[2] = b;
    markDirty(y, x, x);
}

void Canvas::drawPixel(int16_t x, int16_t y, uint16_t color)
{
    uint8_t r, g, b;
    color565to888(color, r, g, b);
    drawPixelRGB888(x, y, r, g, b);
}

void Canvas::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t r, uint8_t g, uint8_t b)
{
    if (pixels == nullptr)
    {
        return;
    }
    int16_t x0 = x < 0 ? 0 : x;
    int16_t y0 = y < 0 ? 0 : y;
    int16_t x1 = x + w > cube::CHAIN_WIDTH ? cube::CHAIN_WIDTH : x + w;
    int16_t y1 = y + h > cube::CHAIN_HEIGHT ? cube::CHAIN_HEIGHT : y + h;
    for (int16_t row = y0; row < y1; row++)
    {
        uint8_t *p = at(x0, row);
        for (int16_t col = x0; col < x1; col++)
        {
            *p++ = r;
            *p++ = g;
            *p++ = b;
        }
        if (x0 < x1)
        {
            markDirty(row, x0, x1 - 1);
        }
    }
}

void Canvas::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
    uint8_t r, g, b;
    color565to888(color, r, g, b);
    fillRect(x, y, w, h, r, g, b);
}

void Canvas::fillScreenRGB888(uint8_t r, uint8_t g, uint8_t b)
{
    fillRect(0, 0, cube::CHAIN_WIDTH, cube::CHAIN_HEIGHT, r, g, b);
}

void Canvas::fillScreen(uint16_t color)
{
    fillRect(0, 0, cube::CHAIN_WIDTH, cube::CHAIN_HEIGHT, color);
}

void Canvas::markAllDirty()
{
    for (int16_t y = 0; y < cube::CHAIN_HEIGHT; y++)
    {
        dirtyFrom[y] = 0;
        dirtyTo[y] = cube::CHAIN_WIDTH - 1;
    }
}

void Canvas::push(MatrixPanel_I2S_DMA &panels)
{
    if (pixels == nullptr)
    {
        return;
    }
    for (int16_t y = 0; y < cube::CHAIN_HEIGHT; y++)
    {
        const int16_t from = dirtyFrom[y];
        const int16_t to = dirtyTo[y];
        if (from > to)
        {
            continue;
        }
        dirtyFrom[y] = cube::CHAIN_WIDTH;
        dirtyTo[y] = -1;
        const uint8_t *p = at(from, y);
        for (int16_t x = from; x <= to; x++, p += 3)
        {
            panels.drawPixelRGB888(x, y, p[0], p[1], p[2]);
        }
    }
}
