#ifndef PLASMA_H
#define PLASMA_H

#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include "config.h"
#include "pattern.h"

class Plasma: public Pattern{
    public:
        Plasma(MatrixPanel_I2S_DMA *display);
        void init();
        void show();
        ~Plasma();

    private:
        MatrixPanel_I2S_DMA *display;
};

#endif