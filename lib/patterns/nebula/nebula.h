#ifndef NEBULA_H
#define NEBULA_H

#include <Arduino.h>
#include "cube_utils.h"
#include "color.h"
#include "noise.h"

/**
 * A slowly drifting 3D noise field, sampled at each pixel's position on the
 * cube surface (cube::toCube), so the cloud flows around the edges as if the
 * cube were cut out of it. Colours from an aurora-like palette.
 *
 * The heaviest per-pixel maths of any pattern (two octaves of Perlin noise
 * for all 12,288 pixels); its tick time in /api/v1/stats is the baseline for
 * deciding whether a fixed-point or SIMD noise is worth writing.
 */
class Nebula : public Pattern
{
public:
    Nebula();
    ~Nebula();
    void begin(PatternServices *services) override;
    void tick() override;
    void end() override;
    uint32_t frameInterval() const override { return 40; }

private:
    // The field is smooth, so it is sampled at every other pixel each way
    // and interpolated within each face: a quarter of the noise evaluations.
    // Sample positions (pre-scaled to noise space) and this frame's values,
    // 96 x 32, in PSRAM.
    static const int SX = cube::CHAIN_WIDTH / 2;
    static const int SY = cube::CHAIN_HEIGHT / 2;
    cube::Vec3 *positions = nullptr;
    float *samples = nullptr;
    uint32_t startMs = 0;
};

#endif
