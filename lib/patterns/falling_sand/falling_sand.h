#ifndef FALLING_SAND_H
#define FALLING_SAND_H

#include <Arduino.h>
#include "cube_utils.h"
#include "color.h"
#include "sand.h"

/**
 * Sand poured from points wandering on the top face runs to the near edges,
 * over them, and piles up on the two side faces, in slowly shifting colours
 * that leave strata. When the piles near the top, the floor opens and it
 * all drains away.
 *
 * The side faces are one 128 x 64 sand grid: chain columns 64-191, with
 * gravity towards chain row 0 (visually down; the side faces are mounted
 * rotated 180 degrees). The seam between them is continuous in chain
 * coordinates, so sand flows across it.
 */
class FallingSand : public Pattern
{
public:
    FallingSand();
    ~FallingSand();
    void begin(PatternServices *services) override;
    void tick() override;
    void end() override;
    uint32_t frameInterval() const override { return 33; }

private:
    static const int GRID_W = 2 * cube::FACE_SIZE;
    static const int GRID_H = cube::FACE_SIZE;
    static const int POURS = 3;
    static const int GRAINS = 64; // in flight on the top face

    struct Pour
    {
        float x, y, vx, vy;
    };
    struct Grain
    {
        cube::Point at;
        cube::Dir dir;
        uint8_t hue;
        bool active;
    };

    uint8_t *grid = nullptr; // hue + 1 per cell, 0 = empty. PSRAM.
    Pour pours[POURS];
    Grain grains[GRAINS];
    uint8_t hue = 0;
    bool draining = false;
    uint32_t frame = 0;
};

#endif
