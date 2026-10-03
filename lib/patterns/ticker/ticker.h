#ifndef TICKER_H
#define TICKER_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <string>
#include "cube_utils.h"
#include "color.h"

/**
 * Large text scrolling right to left across the two side faces (one upright
 * 128-pixel strip, BottomPanels): a custom message if one is set, then the
 * time, then the date, in turn. The top face shows a slowly turning, dim
 * colour wheel.
 */
class Ticker : public Pattern
{
public:
    Ticker();
    ~Ticker();
    void begin(PatternServices *services) override;
    void tick() override;
    void end() override;
    uint32_t frameInterval() const override { return 25; }

    /// The custom message, from the dashboard or /api/v1/ticker. Any task.
    static void setMessage(const std::string &text);
    static std::string message();

private:
    void nextMessage();

    BottomPanels *strip = nullptr;
    uint8_t *topHue = nullptr; // per top-face pixel: angle around its centre
    std::string text;
    int16_t offset = 0;
    int16_t textWidth = 0;
    uint8_t hue = 0;
    int which = 0;
};

#endif
