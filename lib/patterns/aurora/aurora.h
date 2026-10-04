#ifndef AURORA_H
#define AURORA_H

#include <Arduino.h>
#include "cube_utils.h"
#include "color.h"

/**
 * The northern lights. The side faces show them as seen from the ground:
 * wavy curtains along the side strip with a sharp green lower edge, fading
 * to violet as they rise, textured with vertical rays. The top face shows
 * them from below: ribbons drifting and rippling across it, with a few dim
 * stars.
 *
 * The top face's pixel positions (cube::toCube) are worked out once in
 * begin(); the curtains' shape is worked out once per column per frame.
 */
class Aurora : public Pattern
{
public:
    Aurora();
    ~Aurora();
    void begin(PatternServices *services) override;
    void tick() override;
    void end() override;
    // ~12 ms of field plus a ~17 ms full push; the aurora moves slowly.
    uint32_t frameInterval() const override { return 33; }

    static const int RIBBONS = 3;
    static const int CURTAINS = 2;
    static const int STARS = 24;

private:
    struct Place
    {
        int8_t s;  // x - y: along the ribbons, -64..64
        uint8_t w; // x + y: across them, 0..128
    };
    Place *places = nullptr; // per top-face pixel, PSRAM
    uint16_t stars[STARS];   // chain pixel index of each star (top face)
    uint32_t startMs = 0;
};

#endif
