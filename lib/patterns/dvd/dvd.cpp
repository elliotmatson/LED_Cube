#include "dvd.h"

#include <Fonts/FreeSansBold18pt7b.h>

namespace
{
    const int16_t LOGO_W = 28;
    const int16_t LOGO_H = 14;
    // A pixel-art take on the logo: the letters over the disc.
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
    body.x = int32_t(esp_random() % ((strip->width() - LOGO_W) * 256));
    body.y = int32_t(esp_random() % ((strip->height() - LOGO_H) * 256));
    body.vx = (esp_random() & 1 ? SPEED_X : -SPEED_X) * 256;
    body.vy = (esp_random() & 1 ? SPEED_Y : -SPEED_Y) * 256;
    hue = uint8_t(esp_random());
    shownX = int16_t(body.x >> 8);
    shownY = int16_t(body.y >> 8);
    corners = 0;
    flashing = false;
    lastMs = millis();
    const color::RGB c = color::hsv(hue, 255, 255);
    drawLogo(shownX, shownY, Canvas::color565(c.r, c.g, c.b));
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
    const bounce::Hits hits = bounce::step(body, (strip->width() - LOGO_W) * 256, (strip->height() - LOGO_H) * 256, dt);
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

    const int16_t x = int16_t(body.x >> 8), y = int16_t(body.y >> 8);
    if (x != shownX || y != shownY || hits.x || hits.y)
    {
        // Only the logo's old and new rectangles change.
        strip->fillRect(shownX, shownY, LOGO_W, LOGO_H, 0);
        const color::RGB c = color::hsv(hue, 255, 255);
        drawLogo(x, y, Canvas::color565(c.r, c.g, c.b));
        shownX = x;
        shownY = y;
    }
    if (flashing || hits.corner())
    {
        drawTop(now);
    }
}

void Dvd::drawLogo(int16_t x, int16_t y, uint16_t color)
{
    for (int16_t r = 0; r < LOGO_H; r++)
    {
        for (int16_t c = 0; c < LOGO_W; c++)
        {
            if (LOGO[r][c] == '#')
            {
                strip->drawPixel(x + c, y + r, color);
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
