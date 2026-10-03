#ifndef MATRIX_RAIN_H
#define MATRIX_RAIN_H

#include <Arduino.h>
#include "cube_utils.h"

/**
 * Green rain that falls toward the viewer across the top face, pours over
 * the edges and runs down the two side faces (cube::step carries each
 * stream across the seams and turns it to keep going "down").
 */
class MatrixRain : public Pattern
{
public:
    MatrixRain();
    void begin(PatternServices *services) override;
    void tick() override;
    uint32_t frameInterval() const override { return 40; }

private:
    struct Stream
    {
        cube::Point head;
        cube::Dir dir;
        uint16_t speed;    // cells per second
        uint32_t progress; // fractional cells travelled, in 1/1000ths
        bool active;
    };
    static const int STREAMS = 48;

    void spawn(Stream &s);
    Stream streams[STREAMS];
    uint32_t lastMs = 0;
};

#endif
