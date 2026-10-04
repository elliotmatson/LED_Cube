#include "dvd.h"

#include <Fonts/FreeSansBold18pt7b.h>

namespace
{
    const int16_t LOGO_W = 27;
    const int16_t LOGO_H = 14;
    // Where the logo sits in the block.
    const int16_t LOGO_LEFT = 2, LOGO_TOP = 2;
    // A pixel-art take on the logo, cut out of the block: the letters over
    // the disc.
    const char *const LOGO[LOGO_H] = {
        "######...##.....##.######...",
        "##...##..##.....##.##...##..",
        "##....##..##...##..##....##.",
        "##....##..##...##..##....##.",
        "##....##...##.##...##....##.",
        "##....##...##.##...##....##.",
        "##....##....###....##....##.",
        "##...##.....###....##...##..",
        "######.......#.....######...",
        "............................",
        "......################......",
        "..#########......#########..",
        "..#########......#########..",
        "......################......",
    };
    // Pixels per second on each axis. Different, so the path wanders and a
    // corner hit is rare enough to celebrate.
    const int32_t SPEED_X = 26;
    const int32_t SPEED_Y = 19;
    const uint32_t FLASH_MS = 1500;
}

Dvd::Dvd()
{
    data.id = "dvd";
    data.name = "DVD";
}

Dvd::~Dvd()
{
    end();
}

void Dvd::begin(PatternServices *services)
{
    pattern = services;
    strip = new BottomPanels(*pattern->display);
    top = new SinglePanel(*pattern->display, 0, 0);
    for (int16_t y = 0; y < BLOCK_H; y++)
    {
        for (int16_t x = 0; x < BLOCK_W; x++)
        {
            const int16_t lx = x - LOGO_LEFT, ly = y - LOGO_TOP;
            const bool cut = lx >= 0 && lx < LOGO_W && ly >= 0 && ly < LOGO_H && LOGO[ly][lx] == '#';
            coverage[y * BLOCK_W + x] = cut ? 0 : 255;
        }
    }
    body.x = int32_t(esp_random() % ((strip->width() - BLOCK_W) * 256));
    body.y = int32_t(esp_random() % ((strip->height() - BLOCK_H) * 256));
    body.vx = (esp_random() & 1 ? SPEED_X : -SPEED_X) * 256;
    body.vy = (esp_random() & 1 ? SPEED_Y : -SPEED_Y) * 256;
    hue = uint8_t(esp_random());
    shownX = int16_t(body.x >> 8);
    shownY = int16_t(body.y >> 8);
    corners = 0;
    flashing = false;
    lastMs = millis();
    drawBlock();
    drawTop(lastMs);
}

void Dvd::end()
{
    delete strip;
    delete top;
    strip = nullptr;
    top = nullptr;
}

void Dvd::tick()
{
    const uint32_t now = millis();
    // Capped so a stall (a slow push, a flash write) does not teleport it.
    const uint32_t dt = min<uint32_t>(now - lastMs, 100);
    lastMs = now;
    const bounce::Hits hits = bounce::step(body, (strip->width() - BLOCK_W) * 256, (strip->height() - BLOCK_H) * 256, dt);
    if (hits.x || hits.y)
    {
        // A clearly different colour at every bounce.
        hue += 60 + esp_random() % 100;
    }
    if (hits.corner())
    {
        corners++;
        flashing = true;
        flashStartMs = now;
    }

    drawBlock();
    if (flashing || hits.corner())
    {
        drawTop(now);
    }
}

/**
 * Draws the block at its sub-pixel position: each pixel of the box one
 * larger than the block blends the four coverage samples it overlaps
 * (bilinear), so the block's edges and the cut-out move by fractions of a
 * pixel. Only the old box is cleared and the new one written.
 */
void Dvd::drawBlock()
{
    const int16_t ix = int16_t(body.x >> 8), iy = int16_t(body.y >> 8);
    const int32_t fx = body.x & 0xFF, fy = body.y & 0xFF;
    strip->fillRect(shownX, shownY, BLOCK_W + 1, BLOCK_H + 1, 0);
    shownX = ix;
    shownY = iy;

    const color::RGB c = color::hsv(hue, 255, 255);
    auto at = [this](int x, int y) -> int32_t
    {
        return x < 0 || y < 0 || x >= BLOCK_W || y >= BLOCK_H ? 0 : coverage[y * BLOCK_W + x];
    };
    for (int16_t j = 0; j <= BLOCK_H; j++)
    {
        for (int16_t i = 0; i <= BLOCK_W; i++)
        {
            // Output pixel (ix + i) covers source [i - frac, i + 1 - frac):
            // mostly sample i, partly sample i - 1.
            const int32_t v = (at(i, j) * (256 - fx) * (256 - fy) + at(i - 1, j) * fx * (256 - fy) +
                               at(i, j - 1) * (256 - fx) * fy + at(i - 1, j - 1) * fx * fy) >>
                              16;
            if (v > 0)
            {
                strip->drawPixelRGB888(ix + i, iy + j, uint8_t(c.r * v / 255), uint8_t(c.g * v / 255),
                                       uint8_t(c.b * v / 255));
            }
        }
    }
}

/// The corner count, over a flash of the logo's colour fading out after a hit.
void Dvd::drawTop(uint32_t now)
{
    uint8_t level = 0;
    if (flashing)
    {
        const uint32_t t = now - flashStartMs;
        if (t >= FLASH_MS)
        {
            flashing = false;
        }
        else
        {
            level = uint8_t(255 - t * 255 / FLASH_MS);
        }
    }
    const color::RGB c = color::scale(color::hsv(hue, 255, 255), level);
    top->fillScreen(Canvas::color565(c.r, c.g, c.b));

    top->setFont(NULL);
    top->setTextSize(1);
    top->setTextColor(Canvas::color565(140, 140, 140));
    top->setCursor((cube::FACE_SIZE - 7 * 6) / 2, 8);
    top->print("CORNERS");

    char count[12];
    snprintf(count, sizeof(count), "%lu", (unsigned long)corners);
    top->setFont(&FreeSansBold18pt7b);
    int16_t x1, y1;
    uint16_t w, h;
    top->getTextBounds(count, 0, 48, &x1, &y1, &w, &h);
    top->setTextColor(0xFFFF);
    top->setCursor((cube::FACE_SIZE - int16_t(w)) / 2 - x1, 48);
    top->print(count);
    top->setFont(NULL);
}
