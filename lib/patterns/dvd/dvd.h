#ifndef DVD_H
#define DVD_H

#include <Arduino.h>
#include "bounce.h"
#include "cube_utils.h"
#include "color.h"

/**
 * The DVD screensaver: a coloured block with the logo cut out of it bounces
 * around the two side faces (one upright 128-pixel strip, BottomPanels),
 * changing colour at every wall. It is drawn at its exact sub-pixel position,
 * so it glides rather than stepping a whole pixel every few frames. The
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
    uint32_t frameInterval() const override { return 20; }

  static const int16_t BLOCK_W = 31;
  static const int16_t BLOCK_H = 18;

private:
    void drawBlock();
    void drawTop(uint32_t now);

    BottomPanels *strip = nullptr;
    SinglePanel *top = nullptr;
    bounce::Body body;
    // The block's coverage per pixel: 255 inside, 0 where the logo is cut out.
    uint8_t coverage[BLOCK_W * BLOCK_H] = {};
    int16_t shownX = 0, shownY = 0; // top-left of the box last drawn
    uint8_t hue = 0;
    uint32_t lastMs = 0;
    uint32_t corners = 0;
    uint32_t flashStartMs = 0;
    bool flashing = false;
};

#endif
