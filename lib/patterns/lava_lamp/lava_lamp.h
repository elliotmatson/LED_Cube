#ifndef LAVA_LAMP_H
#define LAVA_LAMP_H

#include <Arduino.h>
#include "cube_utils.h"
#include "color.h"

/**
 * A lava lamp. Soft blobs of wax rise and sink through the cube's volume,
 * each at its own pace, wandering sideways; the faces glow where the blobs
 * meet them, and faintly where one is near. The field is each blob's
 * radius squared over its distance squared (metaballs), sampled in the cube
 * (cube::toCube, worked out once in begin()). It is smooth, so as in Nebula
 * it is sampled at every other pixel each way and interpolated within each
 * face: a quarter of the work.
 */
class LavaLamp : public Pattern
{
public:
    LavaLamp();
    ~LavaLamp();
    void begin(PatternServices *services) override;
    void tick() override;
    void end() override;
    uint32_t frameInterval() const override { return 33; }

    static const int BLOBS = 6;

private:
    static const int SX = cube::CHAIN_WIDTH / 2;
    static const int SY = cube::CHAIN_HEIGHT / 2;
    cube::Vec3 *positions = nullptr; // per sample, PSRAM
    float *samples = nullptr;        // this frame's field, per sample
    color::RGB palette[256];         // STOPS as a lookup table
    uint32_t startMs = 0;
};

#endif
