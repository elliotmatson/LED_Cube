#ifndef HYPERSPACE_H
#define HYPERSPACE_H

#include <Arduino.h>
#include "cube_utils.h"

/**
 * Stars streaming outward from the shared corner across all three faces, as
 * if flying down a tunnel. Stars move along rays in the isometric projection
 * (cube::unproject puts them on the right face), speed up as they come
 * nearer, and draw streaks as long as their speed. Every so often the cube
 * jumps to hyperspace: everything accelerates into long streaks, then eases
 * back. Only the streaks are redrawn each frame.
 */
class Hyperspace : public Pattern
{
public:
    Hyperspace();
    ~Hyperspace();
    void begin(PatternServices *services) override;
    void tick() override;
    void end() override;
    uint32_t frameInterval() const override { return 20; }

    static const int STARS = 140;
    static const int MAX_STREAK = 40; // pixels a streak can cover

private:
    struct Star
    {
        float dx = 0, dy = 0; // unit direction in the projected plane
        float r = 0;          // distance from the corner, projected units
        float speed = 1;      // this star's own speed factor
        int16_t drawn = 0;    // pixels written last frame, to clear
    };
    void respawn(Star &s, bool anywhere);
    float warpAt(uint32_t now) const;

    Star *stars = nullptr;       // STARS, PSRAM
    cube::Point *drawn = nullptr; // STARS * MAX_STREAK, PSRAM
    uint32_t lastMs = 0;
    uint32_t warpStartMs = 0;
};

#endif
