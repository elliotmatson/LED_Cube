#ifndef GAME_OF_LIFE_H
#define GAME_OF_LIFE_H

#include <Arduino.h>
#include "cube_utils.h"
#include "life.h"

// a game of life pattern that runs on all 3 sides of the cube

class GameOfLife: public Pattern{
    public:
        GameOfLife();
        void begin(PatternServices *services) override;
        void tick() override;
        uint32_t frameInterval() const override { return 50; }

    private:
        void seed();
        int lastPopulation = -1;
        int unchanged = 0;
        // Row-major, one byte per cell (lib/life's layout).
        uint8_t currentFrame[64 * 64];
        uint8_t nextFrame[64 * 64];
};

#endif