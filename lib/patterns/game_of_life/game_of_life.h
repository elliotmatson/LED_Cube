#ifndef GAME_OF_LIFE_H
#define GAME_OF_LIFE_H

#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include "cube_utils.h"

// a game of life pattern that runs on all 3 sides of the cube

class GameOfLife: public Pattern{
    public:
        GameOfLife();
        ~GameOfLife();
        void init(PatternServices *pattern);
        void start();
        void stop();

    private:
        void show();
        bool currentFrame[64][64];
        bool nextFrame[64][64];
};

#endif