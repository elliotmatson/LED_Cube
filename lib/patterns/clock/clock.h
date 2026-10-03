#ifndef CLOCK_H
#define CLOCK_H

#include <Arduino.h>
#include "cube_utils.h"

class Clock : public Pattern
{
public:
    Clock();
    void begin(PatternServices *services) override;
    void tick() override;
    uint32_t frameInterval() const override { return 1000; }

private:
    struct tm timeinfo;
};

#endif
