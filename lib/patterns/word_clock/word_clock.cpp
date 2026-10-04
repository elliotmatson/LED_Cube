#include "word_clock.h"

#include <math.h>

namespace
{
    const color::RGB UNLIT = {24, 20, 34};
    const color::RGB LIT = {255, 205, 140};
    const uint32_t FADE_MS = 900;
}

WordClock::WordClock()
{
    data.id = "word_clock";
    data.name = "Word Clock";
}

WordClock::~WordClock()
{
    end();
}

void WordClock::begin(PatternServices *services)
{
    pattern = services;
    strip = new BottomPanels(*pattern->display);
    top = new SinglePanel(*pattern->display, 0, 0);
    memset(target, 0, sizeof(target));
    memset(level, 0, sizeof(level));
    memset(drawnLevel, 0xFF, sizeof(drawnLevel)); // force a first draw
    drawnSecond = -1;
    lastMs = millis();
}

void WordClock::end()
{
    delete strip;
    delete top;
    strip = nullptr;
    top = nullptr;
}

/// Letter i in its 8x8 cell, between unlit and lit by its level.
void WordClock::drawLetter(int i)
{
    const int row = i / wordclock::COLS, col = i % wordclock::COLS;
    const color::RGB c = color::lerp(UNLIT, LIT, level[i]);
    // The 5x7 font in a 6x8 box; its background clears the cell.
    strip->drawChar(int16_t(col * 8 + 1), int16_t(row * 8), wordclock::GRID[row][col],
                    Canvas::color565(c.r, c.g, c.b), 0, 1);
    drawnLevel[i] = level[i];
}

/// A ring of 60 seconds, the minute so far filled in, with this second
/// brightest; and a dot in the middle for each minute past the five.
void WordClock::drawTop(const struct tm &t)
{
    top->fillScreen(0);
    // Twelve o'clock points away from the viewer, at the top face's far
    // corner (0, 0); the ring runs clockwise as seen from above.
    const float start = -2.356f; // atan2(-1, -1)
    for (int s = 0; s < 60; s++)
    {
        const float a = start + s * (2 * float(M_PI) / 60);
        const int16_t x = int16_t(lroundf(31.5f + 26 * cosf(a))), y = int16_t(lroundf(31.5f + 26 * sinf(a)));
        color::RGB c = s % 5 == 0 ? color::RGB{70, 60, 90} : color::RGB{30, 26, 40};
        if (s < t.tm_sec)
        {
            c = color::scale(LIT, 110);
        }
        else if (s == t.tm_sec)
        {
            c = LIT;
        }
        top->drawPixelRGB888(x, y, c.r, c.g, c.b);
        if (s == t.tm_sec)
        {
            top->drawPixelRGB888(x + 1, y, c.r, c.g, c.b);
            top->drawPixelRGB888(x, y + 1, c.r, c.g, c.b);
            top->drawPixelRGB888(x + 1, y + 1, c.r, c.g, c.b);
        }
    }
    const int extra = t.tm_min % 5;
    for (int i = 0; i < 4; i++)
    {
        const color::RGB c = i < extra ? LIT : UNLIT;
        top->fillRect(int16_t(23 + i * 5), 30, 3, 3, Canvas::color565(c.r, c.g, c.b));
    }
}

void WordClock::tick()
{
    if (strip == nullptr)
    {
        return;
    }
    const uint32_t now = millis();
    const uint32_t dt = now - lastMs;
    lastMs = now;

    struct tm t;
    // No wait: getLocalTime() otherwise spins while the time is unset.
    const bool known = getLocalTime(&t, 0);
    if (known)
    {
        wordclock::light(t.tm_hour, t.tm_min, target);
        for (int i = 0; i < LETTERS; i++)
        {
            target[i] = target[i] ? 255 : 0;
        }
    }
    else
    {
        memset(target, 0, sizeof(target));
    }

    // Cross-fade towards the target.
    const int step = max<int>(1, int(dt * 255 / FADE_MS));
    for (int i = 0; i < LETTERS; i++)
    {
        if (level[i] < target[i])
        {
            level[i] = uint8_t(min<int>(target[i], level[i] + step));
        }
        else if (level[i] > target[i])
        {
            level[i] = uint8_t(max<int>(target[i], level[i] - step));
        }
        if (level[i] != drawnLevel[i])
        {
            drawLetter(i);
        }
    }

    if (known && t.tm_sec != drawnSecond)
    {
        drawTop(t);
        drawnSecond = t.tm_sec;
    }
}
