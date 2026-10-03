#ifndef WIREFRAMES_H
#define WIREFRAMES_H

#include <Arduino.h>
#include "cube_utils.h"
#include "color.h"

/**
 * Rotating wireframe solids -- cube, octahedron, icosahedron in turn --
 * drawn in the isometric projection (cube::unproject), so they appear to
 * float inside the cube, centred on the shared corner. Nearer edges are
 * brighter.
 */
class Wireframes : public Pattern
{
public:
    Wireframes();
    void begin(PatternServices *services) override;
    void tick() override;
    uint32_t frameInterval() const override { return 33; }

private:
    void drawEdge(const float *a, const float *b, uint8_t hue);
    uint32_t startMs = 0;
};

#endif
