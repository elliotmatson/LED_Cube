#include "renderer.h"

static const uint32_t STATS_INTERVAL_MS = 10000;

bool Renderer::begin(MatrixPanel_I2S_DMA *panels, AsyncWebServer *server, PatternChanged onChanged)
{
    this->panels = panels;
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
    snapshotDone = xSemaphoreCreateBinary();
    snapshotCallers = xSemaphoreCreateMutex();

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

bool Renderer::snapshot(uint8_t *out, TickType_t timeout)
{
    if (out == nullptr || task == nullptr || xTaskGetCurrentTaskHandle() == task)
    {
        return false;
    }
    xSemaphoreTake(snapshotCallers, portMAX_DELAY);
    xSemaphoreTake(snapshotDone, 0); // drop a stale completion
    snapshotTarget = out;
    bool ok = xSemaphoreTake(snapshotDone, timeout) == pdTRUE;
    snapshotTarget = nullptr; // a late copy after a timeout is harmless: same buffer
    xSemaphoreGive(snapshotCallers);
    return ok;
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
    bool running = false;
    uint32_t nextFrame = 0;
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
            wait = remaining > 0 ? pdMS_TO_TICKS(remaining) : 0;
        }

        Command cmd;
        if (xQueueReceive(commands, &cmd, wait) == pdTRUE)
        {
            switch (cmd.type)
            {
            case Command::SWITCH:
                if (running)
                {
                    current->end();
                }
                current = patternList[cmd.index];
                ESP_LOGI("Render", "Starting pattern %s", current->getName().c_str());
                canvas.fillScreen(0);
                current->begin(&services);
                running = true;
                nextFrame = millis();
                if (onChanged)
                {
                    onChanged(current);
                }
                break;
            case Command::STOP:
                if (running)
                {
                    current->end();
                    running = false;
                }
                xSemaphoreGive(ack);
                break;
            case Command::RESUME:
                if (!running && current)
                {
                    // Whatever was drawn on the panels directly is about to
                    // be covered: start from a clean frame.
                    canvas.fillScreen(0);
                    current->begin(&services);
                    running = true;
                    nextFrame = millis();
                }
                break;
            }
            continue;
        }

        // A frame is due.
        uint32_t t0 = micros();
        current->tick();
        uint32_t t1 = micros();
        canvas.push(*panels);
        uint32_t t2 = micros();
        uint8_t *target = snapshotTarget;
        if (target && canvas.data())
        {
            memcpy(target, canvas.data(), Canvas::FRAME_BYTES);
            snapshotTarget = nullptr;
            xSemaphoreGive(snapshotDone);
        }

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
