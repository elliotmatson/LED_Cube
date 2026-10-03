#ifndef CLOCK_H
#define CLOCK_H

#include <Arduino.h>
#include "cube_utils.h"

/**
 * Time across the cube, as seen from the shared corner:
 * - top face: an analog dial, 12 o'clock at the far corner;
 * - right face (1): hours over minutes, large;
 * - left face (2): weekday, day and month.
 * Each part is redrawn only when it changes.
 */
class Clock : public Pattern
{
public:
    Clock();
    ~Clock();
    void begin(PatternServices *services) override;
    void tick() override;
    void end() override;
    uint32_t frameInterval() const override { return 200; }

private:
    void drawDial(const struct tm &t);
    void drawTime(const struct tm &t);
    void drawDate(const struct tm &t);
    void drawWaiting();
    void centered(SinglePanel *face, const char *text, int16_t baseline);

    SinglePanel *timeFace = nullptr;
    SinglePanel *dateFace = nullptr;
    int lastSecond = -1;
    int lastMinute = -1;
    int lastDay = -1;
    bool waitingShown = false;
};

#endif
