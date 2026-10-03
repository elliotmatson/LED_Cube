#ifndef RENDERER_H
#define RENDERER_H

#include <Arduino.h>
#include <functional>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <ESPAsyncWebServer.h>

#include "cube_utils.h"
#include "all_patterns.h"

/**
 * Owns the canvas and the render task: the only place patterns are begun,
 * ticked and ended, and the only writer of the HUB75 buffer while a pattern
 * runs. Each frame is the pattern's tick() into the canvas, then a push of
 * the changed spans to the panels.
 *
 * Other tasks talk to it through a queue: requestPattern() and resume()
 * return at once (safe from AsyncTCP callbacks); stop() waits until
 * rendering has stopped, so the caller can draw on the panels directly.
 */
class Renderer
{
public:
    /// Called from the render task after a pattern has begun.
    using PatternChanged = std::function<void(Pattern *)>;

    /// Timing over the last STATS_INTERVAL_MS window.
    struct Stats
    {
        char pattern[24] = "";
        uint32_t frames = 0;
        uint32_t windowMs = 0;
        uint32_t tickAvgUs = 0, tickMaxUs = 0;
        uint32_t pushAvgUs = 0, pushMaxUs = 0;
    };

    /// Allocates the canvas and starts the render task, idle until the first
    /// requestPattern().
    bool begin(MatrixPanel_I2S_DMA *panels, AsyncWebServer *server, PatternChanged onChanged);

    void requestPattern(size_t index);

    /// The panels' brightness. The renderer owns it so pattern switches can
    /// fade to and from black without fighting the dashboard slider.
    void setBrightness(uint8_t value);
    void stop();
    void resume();
    Stats stats();

private:
    struct Command
    {
        enum Type : uint8_t
        {
            SWITCH, // to patternList[index]
            STOP,   // end the pattern, stop rendering, then give ack
            RESUME, // begin the current pattern again after a STOP
        } type;
        size_t index;
    };

    void loop();
    void startPattern(Pattern *pattern);
    void applyBrightness(uint32_t now);

    // Pattern switches fade out the old pattern, swap at black, and fade in
    // the new one -- both animating throughout.
    enum class Fade : uint8_t
    {
        NONE,
        OUT,
        IN,
    };
    Fade fade = Fade::NONE;
    uint32_t fadeStartMs = 0;
    size_t pendingIndex = 0;
    bool awaitingFirstFrame = false;
    volatile uint8_t brightness = 255;
    bool running = false;
    uint32_t nextFrame = 0;

    MatrixPanel_I2S_DMA *panels = nullptr;
    Canvas canvas;
    PatternServices services;
    PatternChanged onChanged;
    // Only the render task touches this once it has started.
    Pattern *current = nullptr;

    TaskHandle_t task = nullptr;
    QueueHandle_t commands = nullptr;
    SemaphoreHandle_t ack = nullptr;
    SemaphoreHandle_t stopCallers = nullptr; // one stop() at a time

    Stats latest;
    portMUX_TYPE statsMux = portMUX_INITIALIZER_UNLOCKED;
};

#endif
