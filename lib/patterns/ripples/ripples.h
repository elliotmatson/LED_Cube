#ifndef RIPPLES_H
#define RIPPLES_H

#include <Arduino.h>
#include "cube_utils.h"
#include "color.h"

/**
 * Rings spreading out from the shared corner across all three faces, plus
 * raindrops: single rings from random points that cross the seams. Distances
 * are measured in 3D on the cube (cube::toCube), so a ring that reaches an
 * edge carries straight on over it.
 */
class Ripples : public Pattern
{
public:
    Ripples();
    ~Ripples();
    void begin(PatternServices *services) override;
    void tick() override;
    void end() override;
    uint32_t frameInterval() const override { return 25; }

private:
    struct Drop
    {
        cube::Vec3 at;
        uint32_t bornMs;
        uint8_t hue;
        bool active;
    };
    static const int DROPS = 4;

    // Per pixel, fixed for the life of the pattern (PSRAM): its 3D position
    // on the cube, and from its distance to the shared corner, the ring
    // phase (256 per ring), hue offset and fade. Working these out once
    // keeps sinf() and sqrtf() out of the per-frame loop.
    struct Pixel
    {
        uint16_t phase;
        uint8_t hue;
        uint8_t fade;
    };
    cube::Vec3 *positions = nullptr;
    Pixel *pixels = nullptr;
    Drop drops[DROPS];
    uint32_t startMs = 0;
    uint32_t nextDropMs = 0;
};

#endif
