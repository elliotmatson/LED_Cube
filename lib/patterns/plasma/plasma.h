#ifndef PLASMA_H
#define PLASMA_H

#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include "cube_utils.h"

class Plasma: public Pattern{
    public:
        Plasma();
        void init(PatternServices *pattern);
        void start();
        void stop();
        ~Plasma();

    private:
        void show();
};

#endif