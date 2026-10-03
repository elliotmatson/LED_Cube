#include "renderer.h"

static const uint32_t STATS_INTERVAL_MS = 10000;
static const uint32_t FADE_MS = 250;
// While fading, wake at least this often to step the brightness, whatever
// the pattern's frame interval (Clock ticks every 200 ms).
static const uint32_t FADE_STEP_MS = 15;

bool Renderer::begin(MatrixPanel_I2S_DMA *panels, AsyncWebServer *server, PatternChanged onChanged)
{
    this->panels = panels;
    // setBrightness() may have run before the panels were handed over.
    panels->setBrightness8(brightness);
    this->onChanged = onChanged;
    bool ok = canvas.begin();
    if (!ok)
    {
        ESP_LOGE("Render", "No PSRAM for the canvas");
    }
    services.display = &canvas;
    services.server = server;

    commands = xQueueCreate(4, sizeof(Command));
    ack = xSemaphoreCreateBinary();
    stopCallers = xSemaphoreCreateMutex();

    // Core 1, away from WiFi and AsyncTCP on core 0. Above the update and
    // pattern worker tasks (1), below the network stack.
    xTaskCreatePinnedToCore(
        [](void *self)
        { static_cast<Renderer *>(self)->loop(); },
        "Render",
        8192,
        this,
        3,
        &task,
        1);
    return ok;
}

void Renderer::requestPattern(size_t index)
{
    if (index < std::size(patternList))
    {
        Command cmd{Command::SWITCH, index};
        xQueueSend(commands, &cmd, 0);
    }
}

void Renderer::stop()
{
    if (task == nullptr || xTaskGetCurrentTaskHandle() == task)
    {
        return;
    }
    xSemaphoreTake(stopCallers, portMAX_DELAY);
    Command cmd{Command::STOP, 0};
    xQueueSend(commands, &cmd, portMAX_DELAY);
    xSemaphoreTake(ack, portMAX_DELAY);
    xSemaphoreGive(stopCallers);
}

void Renderer::resume()
{
    Command cmd{Command::RESUME, 0};
    xQueueSend(commands, &cmd, 0);
}

void Renderer::setBrightness(uint8_t value)
{
    brightness = value;
    if (fade == Fade::NONE && panels)
    {
        panels->setBrightness8(value);
    }
}

void Renderer::applyBrightness(uint32_t now)
{
    if (fade == Fade::NONE)
    {
        return;
    }
    const uint32_t t = now - fadeStartMs;
    if (fade == Fade::OUT)
    {
        if (t < FADE_MS)
        {
            panels->setBrightness8(uint8_t(brightness * (FADE_MS - t) / FADE_MS));
            return;
        }
        // Black: swap patterns, and fade the new one in.
        panels->setBrightness8(0);
        startPattern(patternList[pendingIndex]);
        fade = Fade::IN;
        fadeStartMs = now;
        return;
    }
    if (t < FADE_MS)
    {
        panels->setBrightness8(uint8_t(brightness * t / FADE_MS));
    }
    else
    {
        panels->setBrightness8(brightness);
        fade = Fade::NONE;
    }
}

void Renderer::startPattern(Pattern *pattern)
{
    if (running)
    {
        current->end();
    }
    current = pattern;
    ESP_LOGI("Render", "Starting pattern %s", current->getName().c_str());
    canvas.fillScreen(0);
    current->begin(&services);
    running = true;
    nextFrame = millis();
    if (onChanged)
    {
        onChanged(current);
    }
}

Renderer::Stats Renderer::stats()
{
    portENTER_CRITICAL(&statsMux);
    Stats copy = latest;
    portEXIT_CRITICAL(&statsMux);
    return copy;
}

void Renderer::loop()
{
    uint32_t statsStart = millis();
    uint32_t frames = 0;
    uint64_t tickTotalUs = 0, pushTotalUs = 0;
    uint32_t tickMaxUs = 0, pushMaxUs = 0;

    for (;;)
    {
        TickType_t wait = portMAX_DELAY;
        if (running)
        {
            int32_t remaining = int32_t(nextFrame - millis());
            if (fade != Fade::NONE && remaining > int32_t(FADE_STEP_MS))
            {
                remaining = FADE_STEP_MS;
            }
            wait = remaining > 0 ? pdMS_TO_TICKS(remaining) : 0;
        }

        Command cmd;
        if (xQueueReceive(commands, &cmd, wait) == pdTRUE)
        {
            switch (cmd.type)
            {
            case Command::SWITCH:
                pendingIndex = cmd.index;
                if (running && fade != Fade::OUT)
                {
                    // Fade the current pattern out first; applyBrightness()
                    // switches once it is black. A request during the fade
                    // just changes what comes next.
                    fade = Fade::OUT;
                    fadeStartMs = millis();
                }
                else if (!running)
                {
                    panels->setBrightness8(0);
                    startPattern(patternList[pendingIndex]);
                    fade = Fade::IN;
                    fadeStartMs = millis();
                }
                break;
            case Command::STOP:
                if (running)
                {
                    current->end();
                    running = false;
                }
                // Updates draw on the panels next, at full brightness.
                fade = Fade::NONE;
                panels->setBrightness8(brightness);
                xSemaphoreGive(ack);
                break;
            case Command::RESUME:
                if (!running && current)
                {
                    // Whatever was drawn on the panels directly is about to
                    // be covered: start from black and fade in.
                    panels->setBrightness8(0);
                    canvas.fillScreen(0);
                    current->begin(&services);
                    running = true;
                    nextFrame = millis();
                    fade = Fade::IN;
                    fadeStartMs = millis();
                }
                break;
            }
            continue;
        }

        applyBrightness(millis());
        if (!running || int32_t(nextFrame - millis()) > 0)
        {
            continue; // woke only to step the fade
        }

        // A frame is due.
        uint32_t t0 = micros();
        current->tick();
        uint32_t t1 = micros();
        canvas.push(*panels);
        uint32_t t2 = micros();

        frames++;
        tickTotalUs += t1 - t0;
        pushTotalUs += t2 - t1;
        tickMaxUs = max(tickMaxUs, t1 - t0);
        pushMaxUs = max(pushMaxUs, t2 - t1);
        if (millis() - statsStart >= STATS_INTERVAL_MS)
        {
            Stats s;
            strlcpy(s.pattern, current->getName().c_str(), sizeof(s.pattern));
            s.frames = frames;
            s.windowMs = millis() - statsStart;
            s.tickAvgUs = tickTotalUs / frames;
            s.tickMaxUs = tickMaxUs;
            s.pushAvgUs = pushTotalUs / frames;
            s.pushMaxUs = pushMaxUs;
            portENTER_CRITICAL(&statsMux);
            latest = s;
            portEXIT_CRITICAL(&statsMux);
            statsStart = millis();
            frames = 0;
            tickTotalUs = pushTotalUs = 0;
            tickMaxUs = pushMaxUs = 0;
        }

        nextFrame += current->frameInterval();
        if (int32_t(millis() - nextFrame) >= 0)
        {
            // Overran: start the next frame now rather than catching up, but
            // yield first so the idle task on this core still runs.
            nextFrame = millis();
            vTaskDelay(1);
        }
    }
}
