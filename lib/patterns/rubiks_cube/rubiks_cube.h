#ifndef RUBIKS_CUBE_H
#define RUBIKS_CUBE_H

#include <Arduino.h>
#include "cube_utils.h"
#include "rubiks.h"

/**
 * The LED cube as the three visible faces of a Rubik's cube: scrambles with
 * random turns, pauses, then plays the scramble back to solved. Stickers are
 * placed by their 3D position (cube::toCube), so edge pieces line up across
 * the seams.
 */
class RubiksCube : public Pattern
{
public:
    RubiksCube();
    ~RubiksCube();
    void begin(PatternServices *services) override;
    void tick() override;
    void end() override;
    uint32_t frameInterval() const override { return 50; }

private:
    enum Phase : uint8_t
    {
        SCRAMBLING,
        SCRAMBLED,
        SOLVING,
        SOLVED,
    };
    static const int SCRAMBLE_MOVES = 25;
    static const uint8_t GAP = 0xFF;

    void draw();

    rubiks::Cube model;
    rubiks::Move moves[SCRAMBLE_MOVES];
    int moveIndex = 0;
    Phase phase = SOLVED;
    uint32_t phaseStartMs = 0;
    uint32_t lastMoveMs = 0;
    // Per pixel: visible face * 9 + row * 3 + column, or GAP. PSRAM.
    uint8_t *slots = nullptr;
};

#endif
