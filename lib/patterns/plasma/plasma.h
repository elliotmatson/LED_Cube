#ifndef PLASMA_H
#define PLASMA_H

#include <Arduino.h>
#include "cube_utils.h"

class Plasma : public Pattern
{
public:
    Plasma();
    ~Plasma();
    void begin(PatternServices *services) override;
    void tick() override;
    void end() override;
    uint32_t frameInterval() const override { return 20; }

private:
    // Each pixel's projected (x, y), offset to be unsigned, row-major. The
    // projection depends only on the pixel, so it is worked out once in
    // begin() rather than for all 12,288 pixels every frame. PSRAM, 24 KB.
    uint8_t *projected = nullptr;
    uint32_t startMs = 0;
};

#endif
