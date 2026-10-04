#include "lava_lamp.h"

#include <math.h>

namespace
{
    // Each blob: radius, and its rise-and-sink and sideways wander (rad/s and
    // starting phase). Periods of a minute or so: a lamp is slow.
    struct Blob
    {
        float radius;
        float riseRate, risePhase;
        float wanderX, phaseX;
        float wanderY, phaseY;
    };
    const Blob BLOBS_DEF[LavaLamp::BLOBS] = {
        {15.0f, 0.110f, 0.0f, 0.050f, 1.0f, 0.070f, 2.0f},
        {12.0f, 0.085f, 2.1f, 0.060f, 3.0f, 0.045f, 0.5f},
        {17.0f, 0.070f, 4.0f, 0.040f, 5.0f, 0.055f, 4.2f},
        {10.0f, 0.130f, 1.3f, 0.075f, 0.3f, 0.065f, 1.7f},
        {13.0f, 0.095f, 5.2f, 0.035f, 2.4f, 0.080f, 3.3f},
        {11.0f, 0.120f, 3.4f, 0.065f, 4.4f, 0.040f, 5.5f},
    };

    // Liquid to wax to hot core.
    const color::RGB STOPS[] = {
        {12, 0, 28},
        {40, 0, 60},
        {200, 30, 10},
        {255, 110, 0},
        {255, 210, 60},
    };
}

LavaLamp::LavaLamp()
{
    data.id = "lava_lamp";
    data.name = "Lava Lamp";
}

LavaLamp::~LavaLamp()
{
    end();
}

void LavaLamp::begin(PatternServices *services)
{
    pattern = services;
    positions = static_cast<cube::Vec3 *>(heap_caps_malloc(SX * SY * sizeof(cube::Vec3), MALLOC_CAP_SPIRAM));
    samples = static_cast<float *>(heap_caps_malloc(SX * SY * sizeof(float), MALLOC_CAP_SPIRAM));
    if (positions == nullptr || samples == nullptr)
    {
        end();
        return;
    }
    for (int sy = 0; sy < SY; sy++)
    {
        for (int sx = 0; sx < SX; sx++)
        {
            positions[sy * SX + sx] = cube::toCube({int16_t(sx * 2), int16_t(sy * 2)});
        }
    }
    for (int i = 0; i < 256; i++)
    {
        palette[i] = color::gradient(STOPS, 5, uint8_t(i));
    }
    startMs = millis();
}

void LavaLamp::end()
{
    free(positions);
    free(samples);
    positions = nullptr;
    samples = nullptr;
}

void LavaLamp::tick()
{
    if (positions == nullptr)
    {
        return;
    }
    const float t = (millis() - startMs) / 1000.0f;

    // Where the blobs are. Kept towards the visible faces (x, y, z = 64), so
    // they meet them rather than hiding in the middle; z sweeps bottom to
    // top and back.
    float bx[BLOBS], by[BLOBS], bz[BLOBS], r2[BLOBS];
    for (int i = 0; i < BLOBS; i++)
    {
        const Blob &b = BLOBS_DEF[i];
        bz[i] = 32 + 34 * sinf(t * b.riseRate + b.risePhase);
        bx[i] = 36 + 26 * sinf(t * b.wanderX + b.phaseX);
        by[i] = 36 + 26 * sinf(t * b.wanderY + b.phaseY);
        r2[i] = b.radius * b.radius;
    }

    for (int i = 0; i < SX * SY; i++)
    {
        const cube::Vec3 &p = positions[i];
        float field = 0;
        for (int k = 0; k < BLOBS; k++)
        {
            const float dx = p.x - bx[k], dy = p.y - by[k], dz = p.z - bz[k];
            field += r2[k] / (dx * dx + dy * dy + dz * dz + 1.0f);
        }
        samples[i] = field;
    }

    for (int16_t row = 0; row < cube::CHAIN_HEIGHT; row++)
    {
        const int sy0 = row / 2;
        // The last row has no sample below it: hold the one above.
        const int sy1 = (row & 1) && sy0 + 1 < SY ? sy0 + 1 : sy0;
        uint8_t *out = pattern->display->rowForWrite(row, 0, cube::CHAIN_WIDTH);
        for (int16_t col = 0; col < cube::CHAIN_WIDTH; col++)
        {
            const int sx0 = col / 2;
            // Interpolate only within a face: the next sample over may be on
            // the neighbouring face, which is not adjacent in 3D.
            const bool lastInFace = (col % cube::FACE_SIZE) == cube::FACE_SIZE - 1;
            const int sx1 = (col & 1) && !lastInFace ? sx0 + 1 : sx0;
            const float field = 0.25f * (samples[sy0 * SX + sx0] + samples[sy0 * SX + sx1] +
                                         samples[sy1 * SX + sx0] + samples[sy1 * SX + sx1]);
            // Below ~0.3 liquid, round 1 the wax's edge, hot only deep inside.
            float v = (field - 0.3f) * 0.32f;
            v = v < 0 ? 0 : (v > 1 ? 1 : v);
            const color::RGB c = palette[uint8_t(v * 255)];
            *out++ = c.r;
            *out++ = c.g;
            *out++ = c.b;
        }
    }
}
