#ifndef WORD_CLOCK_H
#define WORD_CLOCK_H

#include <Arduino.h>
#include "cube_utils.h"
#include "color.h"
#include "wordclock.h"

/**
 * A word clock: a 16 x 8 grid of letters across the side faces (one
 * 128-pixel strip, BottomPanels) with the time to the nearest five minutes
 * lit in warm white (lib/wordclock) and the other letters faintly visible.
 * Changes cross-fade. The top face has a seconds ring and a dot for each
 * minute past the five.
 */
class WordClock : public Pattern
{
public:
    WordClock();
    ~WordClock();
    void begin(PatternServices *services) override;
    void tick() override;
    void end() override;
    uint32_t frameInterval() const override { return 40; }

    static const int LETTERS = wordclock::COLS * wordclock::ROWS;

private:
    void drawLetter(int i);
    void drawTop(const struct tm &t);

    BottomPanels *strip = nullptr;
    SinglePanel *top = nullptr;
    uint8_t target[LETTERS];
    uint8_t level[LETTERS];
    uint8_t drawnLevel[LETTERS];
    int drawnSecond = -1;
    uint32_t lastMs = 0;
};

#endif
