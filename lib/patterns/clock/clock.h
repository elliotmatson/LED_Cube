#ifndef CLOCK_H
#define CLOCK_H

#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include "cube_utils.h"

class Clock: public Pattern{
    public:
        Clock(PatternServices *pattern);
        void init();
        void show();
        ~Clock();
};

#endif