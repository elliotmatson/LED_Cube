#include "plane_sweep.h"

#include <math.h>

namespace
{
    const int PLANES = 3;
    const float THICKNESS = 3.5f; // half-width of the glowing band, cube units
    const uint8_t HUES[PLANES] = {0, 85, 170};
}

PlaneSweep::PlaneSweep()
{
    data.id = "plane_sweep";
    data.name = "Plane Sweep";
}

PlaneSweep::~PlaneSweep()
{
    end();
}

void PlaneSweep::begin(PatternServices *services)
{
    pattern = services;
    positions = static_cast<cube::Vec3 *>(heap_caps_malloc(cube::CELLS * sizeof(cube::Vec3), MALLOC_CAP_SPIRAM));
    if (!positions)
    {
        return;
    }
    for (int i = 0; i < cube::CELLS; i++)
    {
        cube::Vec3 v = cube::toCube({int16_t(i % cube::CHAIN_WIDTH), int16_t(i / cube::CHAIN_WIDTH)});
        positions[i] = {v.x - 32, v.y - 32, v.z - 32};
    }
    startMs = millis();
}

void PlaneSweep::end()
{
    free(positions);
    positions = nullptr;
}

void PlaneSweep::tick()
{
    if (!positions)
    {
        return;
    }
    const float t = (millis() - startMs) / 1000.0f;

    // Each plane: a unit normal that wanders, and an offset that sweeps
    // through the cube (whose corners are ~55 units from its centre).
    float n[PLANES][3], d[PLANES];
    for (int k = 0; k < PLANES; k++)
    {
        const float a = t * (0.13f + 0.05f * k) + k * 2.1f;
        const float b = t * (0.09f + 0.04f * k) + k * 1.3f;
        n[k][0] = cosf(a) * cosf(b);
        n[k][1] = sinf(a) * cosf(b);
        n[k][2] = sinf(b);
        d[k] = 50.0f * sinf(t * (0.45f + 0.12f * k) + k);
    }

    const cube::Vec3 *p = positions;
    for (int16_t y = 0; y < cube::CHAIN_HEIGHT; y++)
    {
        uint8_t *out = pattern->display->rowForWrite(y, 0, cube::CHAIN_WIDTH);
        for (int16_t x = 0; x < cube::CHAIN_WIDTH; x++, p++)
        {
            int r = 0, g = 0, b = 0;
            for (int k = 0; k < PLANES; k++)
            {
                const float dist = fabsf(p->x * n[k][0] + p->y * n[k][1] + p->z * n[k][2] - d[k]);
                if (dist < THICKNESS)
                {
                    const float f = 1.0f - dist / THICKNESS;
                    color::RGB c = color::hsv(HUES[k], 220, uint8_t(255 * f * f));
                    r += c.r;
                    g += c.g;
                    b += c.b;
                }
            }
            *out++ = uint8_t(r > 255 ? 255 : r);
            *out++ = uint8_t(g > 255 ? 255 : g);
            *out++ = uint8_t(b > 255 ? 255 : b);
        }
    }
}
