#include "nebula.h"

// Noise-space units per cube unit: about two features across a face.
static const float SCALE = 1.0f / 24;
// Drift through the field, noise units per second.
static const float DRIFT = 0.08f;

static const color::RGB PALETTE[] = {
    {0, 0, 0},
    {10, 0, 40},
    {30, 20, 140},
    {0, 160, 170},
    {80, 255, 120},
    {255, 255, 255},
};
static const int PALETTE_SIZE = sizeof(PALETTE) / sizeof(PALETTE[0]);

Nebula::Nebula()
{
    data.id = "nebula";
    data.name = "Nebula";
}

Nebula::~Nebula()
{
    end();
}

void Nebula::begin(PatternServices *services)
{
    pattern = services;
    positions = static_cast<cube::Vec3 *>(heap_caps_malloc(SX * SY * sizeof(cube::Vec3), MALLOC_CAP_SPIRAM));
    samples = static_cast<float *>(heap_caps_malloc(SX * SY * sizeof(float), MALLOC_CAP_SPIRAM));
    if (!positions || !samples)
    {
        end();
        return;
    }
    for (int sy = 0; sy < SY; sy++)
    {
        for (int sx = 0; sx < SX; sx++)
        {
            cube::Vec3 v = cube::toCube({int16_t(sx * 2), int16_t(sy * 2)});
            positions[sy * SX + sx] = {v.x * SCALE, v.y * SCALE, v.z * SCALE};
        }
    }
    startMs = millis();
}

void Nebula::end()
{
    free(positions);
    free(samples);
    positions = nullptr;
    samples = nullptr;
}

void Nebula::tick()
{
    if (!positions)
    {
        return;
    }
    const float t = (millis() - startMs) * (DRIFT / 1000.0f);
    for (int i = 0; i < SX * SY; i++)
    {
        const cube::Vec3 &p = positions[i];
        // Drift diagonally through the field so all three faces change.
        samples[i] = noise::fbm(p.x + t, p.y + t * 0.7f, p.z - t * 0.4f, 2);
    }

    for (int16_t y = 0; y < cube::CHAIN_HEIGHT; y++)
    {
        const int sy0 = y / 2;
        // The last row has no sample below it: hold the one above.
        const int sy1 = (y & 1) && sy0 + 1 < SY ? sy0 + 1 : sy0;
        const float fy = (y & 1) ? 0.5f : 0.0f;
        uint8_t *out = pattern->display->rowForWrite(y, 0, cube::CHAIN_WIDTH);
        for (int16_t x = 0; x < cube::CHAIN_WIDTH; x++)
        {
            const int sx0 = x / 2;
            // Interpolate only within a face: the next sample over may be on
            // the neighbouring face, which is not adjacent in 3D.
            const bool lastInFace = (x % cube::FACE_SIZE) == cube::FACE_SIZE - 1;
            const int sx1 = (x & 1) && !lastInFace ? sx0 + 1 : sx0;
            const float fx = (x & 1) ? 0.5f : 0.0f;
            const float top = samples[sy0 * SX + sx0] * (1 - fx) + samples[sy0 * SX + sx1] * fx;
            const float bottom = samples[sy1 * SX + sx0] * (1 - fx) + samples[sy1 * SX + sx1] * fx;
            const float n = top * (1 - fy) + bottom * fy;
            // fbm sits mostly in [-0.6, 0.6]; stretch that over the palette.
            int v = int((n * 0.9f + 0.5f) * 255);
            v = v < 0 ? 0 : (v > 255 ? 255 : v);
            color::RGB c = color::gradient(PALETTE, PALETTE_SIZE, uint8_t(v));
            *out++ = c.r;
            *out++ = c.g;
            *out++ = c.b;
        }
    }
}
