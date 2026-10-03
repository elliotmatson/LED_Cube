#ifndef GAME_OF_LIFE_H
#define GAME_OF_LIFE_H

#include <Arduino.h>
#include "cube_utils.h"
#include "life.h"

/**
 * Conway's Game of Life on all three faces as one surface: cells next to a
 * seam have neighbours on the adjoining face (cube::buildNeighbours), so
 * gliders cross from face to face. Reseeds when the population dies out or
 * stops changing.
 */
class GameOfLife : public Pattern
{
public:
    GameOfLife();
    ~GameOfLife();
    void begin(PatternServices *services) override;
    void tick() override;
    void end() override;
    uint32_t frameInterval() const override { return 50; }

private:
    void seed();

    // PSRAM, allocated in begin(): one byte per cell, row-major 192x64, and
    // the neighbour table (8 indices per cell).
    uint8_t *current = nullptr;
    uint8_t *next = nullptr;
    int16_t *neighbours = nullptr;
    int lastPopulation = -1;
    int unchanged = 0;
};

#endif
