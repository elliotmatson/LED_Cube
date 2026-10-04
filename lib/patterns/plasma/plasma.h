#ifndef PLASMA_H
#define PLASMA_H

#include <Arduino.h>
#include "cube_utils.h"

/**
 * A classic plasma, projected so it flows continuously across the corner,
 * glowing from the shared corner and fading out towards the cube's outer
 * edges.
 */
class Plasma : public Pattern
{
public:
    Plasma();
    ~Plasma();
    void begin(PatternServices *services) override;
    void tick() override;
    void end() override;
    // ~5 ms of maths plus a ~17 ms full push.
    uint32_t frameInterval() const override { return 25; }

private:
    // Each pixel's projected (x, y), offset to be unsigned, row-major. The
    // projection depends only on the pixel, so it is worked out once in
    // begin() rather than for all 12,288 pixels every frame. PSRAM, 24 KB.
    uint8_t *projected = nullptr;
    // Each pixel's brightness, 0-255: full near the three edges that meet at
    // the shared corner, falling to black at the cube's outer edges. PSRAM.
    uint8_t *fade = nullptr;
    uint32_t startMs = 0;
};

#endif
