#ifndef PLASMA_H
#define PLASMA_H

#include <Arduino.h>
#include "cube_utils.h"

class Plasma : public Pattern
{
public:
    Plasma();
    void begin(PatternServices *services) override;
    void tick() override;
    uint32_t frameInterval() const override { return 20; }
};

#endif
