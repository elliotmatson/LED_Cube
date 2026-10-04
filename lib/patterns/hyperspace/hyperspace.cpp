#include "hyperspace.h"

#include <math.h>
#include "color.h"

namespace
{
    // Growth rate of a star's distance: dr/dt = RATE * (r + R0), so stars
    // creep out from the corner and race off the edges, like perspective.
    const float CRUISE_RATE = 1.0f;
    const float WARP_RATE = 3.2f;
    const float R0 = 2.0f;
    // Past the far corners of the hexagon (about 65 projected units).
    const float EXIT_R = 70.0f;
    // A streak shows the last STREAK_S seconds of travel.
    const float STREAK_S = 0.25f;
    // Hyperspace: a jump every WARP_EVERY_MS, WARP_MS long, eased in and out.
    const uint32_t WARP_EVERY_MS = 20000;
    const uint32_t WARP_MS = 3000;

    float frand() { return (esp_random() & 0xFFFF) / 65535.0f; }
}

Hyperspace::Hyperspace()
{
    data.id = "hyperspace";
    data.name = "Hyperspace";
}

Hyperspace::~Hyperspace()
{
    end();
}

void Hyperspace::begin(PatternServices *services)
{
    pattern = services;
    stars = static_cast<Star *>(heap_caps_calloc(STARS, sizeof(Star), MALLOC_CAP_SPIRAM));
    drawn = static_cast<cube::Point *>(heap_caps_malloc(STARS * MAX_STREAK * sizeof(cube::Point), MALLOC_CAP_SPIRAM));
    if (stars == nullptr || drawn == nullptr)
    {
        end();
        return;
    }
    for (int i = 0; i < STARS; i++)
    {
        respawn(stars[i], true);
    }
    lastMs = millis();
    warpStartMs = lastMs + WARP_EVERY_MS / 2; // the first jump comes sooner
}

void Hyperspace::end()
{
    free(stars);
    free(drawn);
    stars = nullptr;
    drawn = nullptr;
}

void Hyperspace::respawn(Star &s, bool anywhere)
{
    const float a = frand() * 2 * float(M_PI);
    s.dx = cosf(a);
    s.dy = sinf(a);
    // At the start, scatter stars along their whole journey so the screen is
    // not empty; afterwards they are born near the corner.
    s.r = anywhere ? frand() * EXIT_R : frand() * 3;
    s.speed = 0.6f + frand() * 0.8f;
    s.drawn = 0;
}

/// How far into hyperspace the cube is, 0-1, eased at both ends.
float Hyperspace::warpAt(uint32_t now) const
{
    const int32_t t = int32_t(now - warpStartMs);
    if (t < 0 || t >= int32_t(WARP_MS))
    {
        return 0;
    }
    const float x = float(t) / WARP_MS; // 0 -> 1
    const float hump = sinf(x * float(M_PI));
    return hump * hump;
}

void Hyperspace::tick()
{
    if (stars == nullptr)
    {
        return;
    }
    const uint32_t now = millis();
    const float dt = min<uint32_t>(now - lastMs, 100) / 1000.0f;
    lastMs = now;
    if (int32_t(now - warpStartMs) >= int32_t(WARP_MS))
    {
        warpStartMs += WARP_EVERY_MS;
    }
    const float warp = warpAt(now);
    const float rate = CRUISE_RATE + (WARP_RATE - CRUISE_RATE) * warp;

    Canvas &canvas = *pattern->display;
    // Clear every old streak before drawing any new one, so a streak drawn
    // earlier this frame is never erased by a neighbour's clean-up.
    for (int i = 0; i < STARS; i++)
    {
        const cube::Point *p = drawn + i * MAX_STREAK;
        for (int k = 0; k < stars[i].drawn; k++)
        {
            canvas.drawPixelRGB888(p[k].x, p[k].y, 0, 0, 0);
        }
    }

    for (int i = 0; i < STARS; i++)
    {
        Star &s = stars[i];
        const float v = rate * s.speed * (s.r + R0); // projected units/s
        s.r += v * dt;
        if (s.r > EXIT_R)
        {
            respawn(s, false);
            continue;
        }
        // Brighter as it comes nearer, brighter still in hyperspace.
        const float head = min(1.0f, 0.35f + s.r / 35.0f + warp * 0.3f);
        const float length = min(float(MAX_STREAK) * 0.9f, max(0.8f, v * STREAK_S));
        cube::Point *out = drawn + i * MAX_STREAK;
        int count = 0;
        cube::Point last = cube::NO_POINT;
        // From the tail to the head, half a unit at a time; skip repeats.
        for (float d = length; d >= 0 && count < MAX_STREAK; d -= 0.5f)
        {
            const float r = s.r - d;
            if (r < 0)
            {
                continue;
            }
            const cube::Point p = cube::unproject(s.dx * r, s.dy * r);
            if (!p.valid() || (p.x == last.x && p.y == last.y))
            {
                continue;
            }
            last = p;
            // Blue at the tail, white at the head.
            const float t = 1.0f - d / (length + 0.001f);
            const float level = head * t * t;
            const color::RGB c = color::lerp({60, 90, 255}, {255, 255, 255}, uint8_t(t * 255));
            canvas.drawPixelRGB888(p.x, p.y, uint8_t(c.r * level), uint8_t(c.g * level), uint8_t(c.b * level));
            out[count++] = p;
        }
        s.drawn = int16_t(count);
    }
}
