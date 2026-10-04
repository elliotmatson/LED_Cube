#ifndef LOST_H
#define LOST_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <string>
#include "cube_utils.h"

/**
 * Shown in lost mode (see LostMode), and never listed or selectable
 * otherwise: the owner's message scrolling round the side faces in large
 * type, and LOST flashing on top with the cube's short ID, so whoever has it
 * can quote that when they get in touch.
 */
class LostPattern : public Pattern
{
public:
    LostPattern();
    ~LostPattern();
    void begin(PatternServices *services) override;
    void tick() override;
    void end() override;
    uint32_t frameInterval() const override { return 25; }

    /// The message and the ID shown under LOST. Any task.
    static void setMessage(const std::string &text);
    static void setLabel(const std::string &label);

private:
    void drawTop(bool on);

    BottomPanels *strip = nullptr;
    SinglePanel *top = nullptr;
    std::string text;
    int16_t offset = 0;
    int16_t textWidth = 0;
    int shownBlink = -1;
};

#endif
