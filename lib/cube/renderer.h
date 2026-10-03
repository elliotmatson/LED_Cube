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
    void stop();
    void resume();
    Stats stats();

    /**
     * Copies the next complete frame -- taken right after a push, so never
     * half-drawn -- into `out` (Canvas::FRAME_BYTES). For capturing what the
     * cube shows (scripts/capture_patterns.py). Blocks for up to a frame.
     *
     * @return false if no pattern is rendering or it timed out.
     */
    bool snapshot(uint8_t *out, TickType_t timeout);

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

    // A pending snapshot: the render task fills it after its next push.
    uint8_t *volatile snapshotTarget = nullptr;
    SemaphoreHandle_t snapshotDone = nullptr;
    SemaphoreHandle_t snapshotCallers = nullptr;

    Stats latest;
    portMUX_TYPE statsMux = portMUX_INITIALIZER_UNLOCKED;
};

#endif
