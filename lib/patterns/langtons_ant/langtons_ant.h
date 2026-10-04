#ifndef LANGTONS_ANT_H
#define LANGTONS_ANT_H

#include <Arduino.h>
#include "cube_utils.h"
#include "color.h"
#include "langton.h"

/**
 * Langton's ant and its multi-colour cousins (lib/langton) on the whole
 * surface, in 2x2-pixel cells that join across the seams
 * (cube::buildBlockNeighbours). A few ants share one rule, picked at random,
 * start slowly enough to follow and speed up; after a few minutes, or once
 * most of the cube is coloured, everything fades and a new rule starts.
 */
class LangtonsAnt : public Pattern
{
public:
    LangtonsAnt();
    ~LangtonsAnt();
    void begin(PatternServices *services) override;
    void tick() override;
    void end() override;
    uint32_t frameInterval() const override { return 33; }

    static const int16_t BLOCK = 2;
    static const int ANTS = 3;

private:
    void restart();
    void drawCell(int cell, color::RGB c);
    color::RGB stateColor(uint8_t state, uint8_t level = 255) const;

    int cells = 0;
    int16_t *nbr = nullptr;
    uint8_t *grid = nullptr;
    langton::Rule rule;
    langton::Ant ants[ANTS];
    int coloured = 0; // cells not in state 0
    uint8_t hue = 0;
    bool fading = false;
    uint32_t startMs = 0, fadeStartMs = 0;
};

#endif
