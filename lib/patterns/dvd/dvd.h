#ifndef DVD_H
#define DVD_H

#include <Arduino.h>
#include "bounce.h"
#include "cube_utils.h"
#include "color.h"

/**
 * The DVD screensaver: the logo bounces around the two side faces (one
 * upright 128-pixel strip, BottomPanels), changing colour at every wall. The
 * top face counts the times it hits a corner exactly, and flashes the
 * logo's colour when it does.
 */
class Dvd : public Pattern
{
public:
    Dvd();
    ~Dvd();
    void begin(PatternServices *services) override;
    void tick() override;
    void end() override;
    uint32_t frameInterval() const override { return 33; }

private:
    void drawLogo(int16_t x, int16_t y, uint16_t color);
    void drawTop(uint32_t now);

    BottomPanels *strip = nullptr;
    SinglePanel *top = nullptr;
    bounce::Body body;
    int16_t shownX = 0, shownY = 0;
    uint8_t hue = 0;
    uint32_t lastMs = 0;
    uint32_t corners = 0;
    uint32_t flashStartMs = 0;
    bool flashing = false;
};

#endif
