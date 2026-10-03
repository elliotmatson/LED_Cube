#include "ripples.h"

#include <math.h>

// Ring geometry, in cube units (one pixel).
static const float RING_SPACING = 14.0f;   // between the corner's rings
static const float RING_SPEED = 0.012f;    // corner rings, units per ms
static const float DROP_SPEED = 0.03f;     // raindrop ring, units per ms
static const float DROP_WIDTH = 3.0f;      // half-width of a raindrop ring
static const uint32_t DROP_LIFE_MS = 4000; // fades out over this

Ripples::Ripples()
{
    data.id = "ripples";
    data.name = "Ripples";
}

Ripples::~Ripples()
{
    end();
}

void Ripples::begin(PatternServices *services)
{
    pattern = services;
    positions = static_cast<cube::Vec3 *>(heap_caps_malloc(cube::CELLS * sizeof(cube::Vec3), MALLOC_CAP_SPIRAM));
    pixels = static_cast<Pixel *>(heap_caps_malloc(cube::CELLS * sizeof(Pixel), MALLOC_CAP_SPIRAM));
    if (!positions || !pixels)
    {
        end();
        return;
    }
    for (int cell = 0; cell < cube::CELLS; cell++)
    {
        cube::Vec3 v = cube::toCube({int16_t(cell % cube::CHAIN_WIDTH), int16_t(cell / cube::CHAIN_WIDTH)});
        positions[cell] = v;
        const float dx = 64 - v.x, dy = 64 - v.y, dz = 64 - v.z;
        const float d = sqrtf(dx * dx + dy * dy + dz * dz);
        pixels[cell].phase = uint16_t(d / RING_SPACING * 256);
        pixels[cell].hue = uint8_t(d * 2);
        pixels[cell].fade = uint8_t(255 * fmaxf(0.0f, 1.0f - d / 110.0f));
    }
    for (Drop &d : drops)
    {
        d.active = false;
    }
    startMs = millis();
    nextDropMs = startMs + 1500;
}

void Ripples::end()
{
    free(positions);
    free(pixels);
    positions = nullptr;
    pixels = nullptr;
}

void Ripples::tick()
{
    if (!positions)
    {
        return;
    }
    const uint32_t now = millis();
    // Rings move outward: phase 256 is one ring spacing.
    const uint16_t shift = uint16_t((now - startMs) * RING_SPEED / RING_SPACING * 256);
    const uint8_t baseHue = uint8_t((now - startMs) / 120);

    // A new raindrop every second or two, at a random pixel.
    if (int32_t(now - nextDropMs) >= 0)
    {
        for (Drop &d : drops)
        {
            if (!d.active || now - d.bornMs > DROP_LIFE_MS)
            {
                d.at = positions[random(cube::CELLS)];
                d.bornMs = now;
                d.hue = uint8_t(random(256));
                d.active = true;
                break;
            }
        }
        nextDropMs = now + 1000 + random(1500);
    }

    // Each live raindrop ring as a band of squared distances, so most pixels
    // are ruled out without a square root.
    struct Band
    {
        cube::Vec3 at;
        float inner2, outer2, radius, strength;
        uint8_t hue;
    } bands[DROPS];
    int live = 0;
    for (const Drop &d : drops)
    {
        const uint32_t age = now - d.bornMs;
        if (!d.active || age > DROP_LIFE_MS)
        {
            continue;
        }
        Band &b = bands[live++];
        b.at = d.at;
        b.radius = age * DROP_SPEED;
        const float inner = fmaxf(0.0f, b.radius - DROP_WIDTH), outer = b.radius + DROP_WIDTH;
        b.inner2 = inner * inner;
        b.outer2 = outer * outer;
        b.strength = 1.0f - float(age) / DROP_LIFE_MS;
        b.hue = d.hue;
    }

    const Pixel *px = pixels;
    const cube::Vec3 *pos = positions;
    for (int16_t y = 0; y < cube::CHAIN_HEIGHT; y++)
    {
        uint8_t *out = pattern->display->rowForWrite(y, 0, cube::CHAIN_WIDTH);
        for (int16_t x = 0; x < cube::CHAIN_WIDTH; x++, px++, pos++)
        {
            // Soft rings: the cosine table squared, faded with distance.
            const uint16_t w = fast_cos(uint16_t(px->phase - shift));
            const uint8_t v = uint8_t((((w * w) >> 8) * px->fade * 180) >> 16);
            color::RGB c = color::hsv(uint8_t(baseHue + px->hue), 255, v);

            for (int i = 0; i < live; i++)
            {
                const Band &b = bands[i];
                const float dx = pos->x - b.at.x, dy = pos->y - b.at.y, dz = pos->z - b.at.z;
                const float d2 = dx * dx + dy * dy + dz * dz;
                if (d2 < b.inner2 || d2 > b.outer2)
                {
                    continue;
                }
                const float off = fabsf(sqrtf(d2) - b.radius);
                const uint8_t rv = uint8_t(255 * b.strength * fmaxf(0.0f, 1.0f - off / DROP_WIDTH));
                color::RGB ring = color::hsv(b.hue, 120, rv);
                c.r = uint8_t(min(255, c.r + ring.r));
                c.g = uint8_t(min(255, c.g + ring.g));
                c.b = uint8_t(min(255, c.b + ring.b));
            }
            *out++ = c.r;
            *out++ = c.g;
            *out++ = c.b;
        }
    }
}
