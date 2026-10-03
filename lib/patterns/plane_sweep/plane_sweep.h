#ifndef PLANE_SWEEP_H
#define PLANE_SWEEP_H

#include <Arduino.h>
#include "cube_utils.h"
#include "color.h"

/**
 * Three coloured planes sweeping back and forth through the cube's volume
 * while slowly tilting; each glows where it cuts the cube's surface, so the
 * lines it draws bend over the edges.
 */
class PlaneSweep : public Pattern
{
public:
    PlaneSweep();
    ~PlaneSweep();
    void begin(PatternServices *services) override;
    void tick() override;
    void end() override;
    uint32_t frameInterval() const override { return 25; }

private:
    cube::Vec3 *positions = nullptr; // each pixel, centred on the cube. PSRAM.
    uint32_t startMs = 0;
};

#endif
