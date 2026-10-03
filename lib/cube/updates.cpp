#include "updates.h"

#include <esp_app_desc.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <Fonts/FreeSansBold12pt7b.h>

#include "LEMONMILK_Medium7pt7b.h"

// The cube's custom app descriptor, placed right after the standard one in
// every image (ESP-IDF's .rodata_custom_desc). Its cookie is the "signature"
// checked when "Signed FW only" is on. It is a fixed string, not a
// cryptographic signature: it tells cube firmware from other firmware, and
// does not stop anyone determined.
const __attribute__((section(".rodata_custom_desc"))) CubePartition cubePartition = {CUBE_MAGIC_COOKIE};

bool OtaWriter::begin()
{
    abort();
    bytes = 0;
    partition = esp_ota_get_next_update_partition(NULL);
    if (partition == nullptr)
    {
        err = ESP_ERR_NOT_FOUND;
        return false;
    }
    err = esp_ota_begin(partition, OTA_WITH_SEQUENTIAL_WRITES, &handle);
    if (err != ESP_OK)
    {
        handle = 0;
        return false;
    }
    return true;
}

bool OtaWriter::write(const uint8_t *data, size_t len)
{
    if (!handle)
    {
        return false;
    }
    err = esp_ota_write(handle, data, len);
    if (err != ESP_OK)
    {
        abort();
        return false;
    }
    bytes += len;
    return true;
}

bool OtaWriter::finish()
{
    if (!handle)
    {
        return false;
    }
    err = esp_ota_end(handle); // frees the handle whatever the result
    handle = 0;
    if (err == ESP_OK)
    {
        err = esp_ota_set_boot_partition(partition);
    }
    return err == ESP_OK;
}

void OtaWriter::abort()
{
    if (handle)
    {
        esp_ota_abort(handle);
        handle = 0;
    }
}

Updates::Updates(ESPDash &dash)
    : dashboard(&dash),
      firmwareUploadCard(dash, "Update Firmware", ".bin"),
      firmwareUploadStatus(dash, "Update Status", dash::Status::NONE)
{
}

void Updates::begin(AsyncWebServer &server, MatrixPanel_I2S_DMA *panels, Settings &settings, Renderer &renderer, dash::Tab &tab)
{
    this->server = &server;
    this->panels = panels;
    this->settings = &settings;
    this->renderer = &renderer;
    firmwareUploadCard.setTab(tab);
    firmwareUploadStatus.setTab(tab);
    // The card's value is where its frontend POSTs the file, as multipart
    // field "file"; its progress ring follows the response status.
    firmwareUploadCard.setValue(FIRMWARE_UPLOAD_ROUTE);
    initFirmwareUpload();
    setOta(settings.ota());
    setGithub(settings.github());
}

firmware_image::Expected Updates::expectedImage()
{
    return firmware_image::Expected{
        CONFIG_IDF_FIRMWARE_CHIP_ID,
        // Compared with the running image rather than a literal, so renaming
        // the project cannot silently start rejecting (or accepting) the
        // wrong thing.
        esp_app_get_description()->project_name,
        settings->signedFirmwareOnly() ? cubePartition.cookie : nullptr,
    };
}

/**
 * Checks the image ArduinoOTA or the GitHub updater has just finished writing
 * to the next OTA partition, and aborts the update if it fails. Those two
 * paths stream into Update without a look at the first chunk, so this runs at
 * the end instead -- before Update.end(), while the first 16 bytes are still
 * held back, hence checkDescriptor(). end() checks the header (chip id
 * included) itself.
 */
void Updates::verifyWrittenImage()
{
    uint8_t head[firmware_image::CHECK_BYTES];
    const esp_partition_t *next = esp_ota_get_next_update_partition(NULL);
    if (next == nullptr || esp_partition_read(next, 0, head, sizeof(head)) != ESP_OK)
    {
        ESP_LOGE("Updates", "Could not read back the new image");
        Update.abort();
        return;
    }
    const char *problem = firmware_image::checkDescriptor(head, sizeof(head), expectedImage());
    if (problem)
    {
        ESP_LOGE("Updates", "Rejecting update: %s", problem);
        Update.abort();
    }
}

namespace
{
    const char *WORD = "UPDATE";
    const int16_t ROW_SPACING = 16;
    const int16_t WORD_GAP = 12;
    const uint32_t FRAME_MS = 50;
    // Slept after every frame, however long it took. Flash writes stall the
    // caches, so during an update a frame can overrun its slot; a task that
    // then never blocks starves the idle task on its core until the task
    // watchdog fires -- and the watchdog's own backtrace, printed mid-write,
    // panicked the cube.
    const uint32_t MIN_REST_MS = 10;

    // "UPDATE" in rows across a face, each row scrolling the opposite way to
    // the one above it. `pixelsPerSecond` sets the speed; `firstLeft` the
    // direction of the top row.
    void tile(SinglePanel &face, int16_t wordWidth, uint32_t ms, int pixelsPerSecond, bool firstLeft, uint16_t color)
    {
        const int period = wordWidth + WORD_GAP;
        const int travel = int((uint64_t(ms) * pixelsPerSecond / 1000) % period);
        face.setFont(&LEMONMILK_Medium7pt7b);
        face.setTextSize(1);
        face.setTextWrap(false);
        face.setTextColor(color);
        for (int row = 0; row <= cube::FACE_SIZE / ROW_SPACING; row++)
        {
            const bool left = ((row & 1) == 0) == firstLeft;
            const int y = row * ROW_SPACING + 11;
            // Start each row somewhere different so the words do not line up.
            const int start = (row * period / 3 + (left ? period - travel : travel)) % period;
            for (int x = start - period; x < cube::FACE_SIZE; x += period)
            {
                face.setCursor(int16_t(x), int16_t(y));
                face.print(WORD);
            }
        }
        face.setFont(NULL);
    }
}

void Updates::startAnimation()
{
    canvasReady = canvas.begin();
    if (!canvasReady || animationTask)
    {
        return;
    }
    if (!animationDone)
    {
        animationDone = xSemaphoreCreateBinary();
    }
    animationStartMs = millis();
    progressPermille = 0;
    animating = true;
    // Core 1, which the render task leaves idle during an update and AsyncTCP
    // no longer uses (CONFIG_ASYNC_TCP_RUNNING_CORE=0), above the ordinary
    // background tasks so it stays smooth. The upload itself runs on core 0.
    xTaskCreatePinnedToCore(
        [](void *self)
        { static_cast<Updates *>(self)->animationLoop(); },
        "Update screen", 4096, this, 6, &animationTask, 1);
}

void Updates::animationLoop()
{
    while (animating)
    {
        const uint32_t start = millis();
        drawFrame();
        const uint32_t spent = millis() - start;
        vTaskDelay(pdMS_TO_TICKS(spent + MIN_REST_MS < FRAME_MS ? FRAME_MS - spent : MIN_REST_MS));
    }
    xSemaphoreGive(animationDone);
    vTaskDelete(NULL);
}

void Updates::stopAnimation()
{
    if (!animationTask)
    {
        return;
    }
    animating = false;
    xSemaphoreTake(animationDone, pdMS_TO_TICKS(500));
    animationTask = nullptr;
}

void Updates::abandon()
{
    stopAnimation();
    renderer->resume();
}

void Updates::setProgress(float fraction)
{
    fraction = fraction < 0 ? 0 : (fraction > 1 ? 1 : fraction);
    progressPermille = uint16_t(fraction * 1000);
}

/// One frame of the update screen.
void Updates::drawFrame()
{
    if (!canvasReady)
    {
        return;
    }
    const uint32_t t = millis() - animationStartMs;
    const float fraction = progressPermille / 1000.0f;

    canvas.fillScreen(0);
    SinglePanel top(canvas, 0, 0);
    SinglePanel right(canvas, 1, 2); // upright, as the patterns use them
    SinglePanel left(canvas, 2, 2);
    int16_t x1, y1;
    uint16_t w, h;
    top.setFont(&LEMONMILK_Medium7pt7b);
    top.getTextBounds(WORD, 0, 11, &x1, &y1, &w, &h);
    const int16_t wordWidth = int16_t(w) + x1;
    const uint16_t dim = Canvas::color565(0, 85, 115);

    // Rows alternate direction on every face; the faces differ in speed and
    // in which way their top row goes.
    tile(top, wordWidth, t, 22, true, dim);
    tile(right, wordWidth, t, 30, false, dim);
    tile(left, wordWidth, t, 30, true, dim);

    // The percentage, large, over the top face.
    char percent[6];
    snprintf(percent, sizeof(percent), "%d%%", int(fraction * 100 + 0.5f));
    top.setFont(&FreeSansBold12pt7b);
    top.getTextBounds(percent, 0, 40, &x1, &y1, &w, &h);
    const int16_t px = (cube::FACE_SIZE - int16_t(w)) / 2 - x1;
    top.fillRect(px + x1 - 3, y1 - 3, w + 6, h + 6, 0);
    top.setTextColor(0xFFFF);
    top.setCursor(px, 40);
    top.print(percent);
    top.setFont(NULL);

    drawEdgeProgress(int(fraction * 512));
    canvas.push(*panels);
}

/// The progress line: 512 steps around the cube's edges, as before.
void Updates::drawEdgeProgress(int i)
{
    Canvas &c = canvas;
    const uint16_t white = 0xFFFF;
    c.drawFastHLine(128, 0, constrain(i, 0, 64), white);
    c.drawFastVLine(191, 0, constrain(i - 64, 0, 64), white);

    c.drawFastVLine(0, 64 - constrain(i - 128, 0, 63), constrain(i - 128, 0, 64), white);
    c.drawFastHLine(0, 0, constrain(i - 192, 0, 64), white);

    c.drawFastVLine(64, 64 - constrain(i - 256, 0, 63), constrain(i - 256, 0, 64), white);
    c.drawFastHLine(64, 0, constrain(i - 320, 0, 64), white);

    c.drawFastVLine(127, 0, constrain(i - 384, 0, 64), white);
    c.drawFastVLine(128, 0, constrain(i - 384, 0, 64), white);

    c.drawFastHLine(128 - constrain(i - 448, 0, 63), 63, constrain(i - 448, 0, 64), white);
    c.drawFastHLine(128, 63, constrain(i - 448, 0, 64), white);
    c.drawFastHLine(64 - constrain(i - 448, 0, 64), 63, constrain(i - 448, 0, 64), white);
    c.drawFastVLine(63, 64 - constrain(i - 448, 0, 64), constrain(i - 448, 0, 64), white);
}

/// Progress from ArduinoOTA or the GitHub updater.
void Updates::onProgress(unsigned int progress, unsigned int total)
{
    ESP_LOGD("Updates", "Progress: %u%%", total ? (progress * 100) / total : 0);
    if (settings->signedFirmwareOnly() && progress == total)
    {
        verifyWrittenImage();
    }
    setProgress(total ? float(progress) / total : 0);
}

/// Dims the panels to black before the restart into new firmware.
void Updates::fadeOut()
{
    settings->flush();
    // A real fade, about 0.6 s, all the way to black. The old loop had no
    // delay and stopped one step short of 0, so the last frame stayed faintly
    // lit until the restart.
    const int start = settings->brightness();
    const int STEPS = 30;
    for (int step = 1; step <= STEPS; step++)
    {
        panels->setBrightness8(uint8_t(start * (STEPS - step) / STEPS));
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    stopAnimation();
    panels->clearScreen();
    panels->setBrightness8(0);
}

void Updates::setOta(bool ota)
{
    settings->setOta(ota);
    if (ota)
    {
        if (otaTask)
        {
            // Already running. begin() twice would start a second listener,
            // and a second task would lose the first one's handle.
            return;
        }
        ESP_LOGI("Updates", "Starting ArduinoOTA");
        ArduinoOTA.setHostname(HOSTNAME);
        ArduinoOTA
            .onStart([this]()
                     {
                ESP_LOGI("Updates", "ArduinoOTA update starting");
                renderer->stop();
                startAnimation(); })
            .onEnd([this]()
                   { fadeOut(); })
            .onProgress([this](unsigned int progress, unsigned int total)
                        { onProgress(progress, total); })
            .onError([this](ota_error_t error)
                     {
                ESP_LOGE("Updates", "ArduinoOTA error %u", error);
                // The pattern was stopped in onStart; nothing will reboot
                // into new firmware now, so bring it back.
                abandon(); });
        ArduinoOTA.begin();
        xTaskCreate(
            [](void *self)
            { static_cast<Updates *>(self)->checkForOTA(); },
            "ArduinoOTA",
            6000,
            this,
            5,
            &otaTask);
    }
    else if (otaTask)
    {
        ESP_LOGI("Updates", "Stopping ArduinoOTA");
        vTaskDelete(otaTask);
        otaTask = nullptr;
        ArduinoOTA.end();
    }
}

void Updates::setGithub(bool github)
{
    settings->setGithub(github);
    if (github)
    {
        if (githubTask)
        {
            return; // already checking
        }
        ESP_LOGI("Updates", "GitHub updates enabled");
        xTaskCreate(
            [](void *self)
            { static_cast<Updates *>(self)->checkForUpdates(); },
            "GitHub Updates",
            8000,
            this,
            5,
            &githubTask);
    }
    else if (githubTask)
    {
        ESP_LOGI("Updates", "GitHub updates disabled");
        vTaskDelete(githubTask);
        githubTask = nullptr;
    }
}

/**
 * Serves the dashboard's firmware upload card. The image is streamed straight
 * into the inactive OTA partition as each chunk arrives; the cube restarts into
 * it once OtaWriter::finish() has checked it and marked it bootable.
 */
void Updates::initFirmwareUpload()
{
    server->on(
        FIRMWARE_UPLOAD_ROUTE, HTTP_POST,
        [this](AsyncWebServerRequest *request)
        {
            if (updateRequest != request)
            {
                // Rejected while the body arrived -- that send() only queued
                // the response, so isSent() is still false here and the queued
                // one must not be replaced -- or a POST with no file at all.
                if (request->getResponse() == nullptr)
                {
                    request->send(400, "text/plain", "No firmware file in the request");
                }
                return;
            }
            updateRequest = nullptr;

            // finish() verifies the image and sets the boot partition.
            if (!cardWriter.finish())
            {
                ESP_LOGE(__func__, "Image rejected: %s", cardWriter.error());
                abandon();
                firmwareUploadStatus.setFeedback("The uploaded image is not valid firmware", dash::Status::DANGER);
                dashboard->sendUpdates();
                request->send(400, "text/plain", String("Image rejected: ") + cardWriter.error());
                return;
            }

            ESP_LOGI(__func__, "Firmware update staged, restarting");
            setProgress(1.0f);
            firmwareUploadStatus.setFeedback("Update complete - restarting", dash::Status::SUCCESS);
            dashboard->sendUpdates();
            AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "Update complete, restarting");
            response->addHeader("Connection", "close");
            request->send(response);
            // Fade out and restart from a task of its own, so the response and
            // the dashboard update go out meanwhile.
            xTaskCreate(
                [](void *self)
                {
                    vTaskDelay(pdMS_TO_TICKS(300));
                    static_cast<Updates *>(self)->fadeOut();
                    ESP.restart();
                },
                "Restart", 3072, this, 1, nullptr);
        },
        [this](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
        {
            if (index == 0)
            {
                ESP_LOGI(__func__, "Firmware upload starting: %s", filename.c_str());
                if (updateRequest != nullptr)
                {
                    request->send(409, "text/plain", "An update is already in progress");
                    return;
                }
                if (!filename.endsWith(".bin"))
                {
                    request->send(400, "text/plain", "Only .bin files are accepted");
                    return;
                }
                const char *problem = firmware_image::check(data, len, expectedImage());
                if (problem)
                {
                    ESP_LOGE(__func__, "Rejecting %s: %s", filename.c_str(), problem);
                    firmwareUploadStatus.setFeedback(problem, dash::Status::WARNING);
                    dashboard->sendUpdates();
                    request->send(400, "text/plain", problem);
                    return;
                }
                if (!cardWriter.begin())
                {
                    ESP_LOGE(__func__, "OTA begin failed: %s", cardWriter.error());
                    request->send(500, "text/plain", cardWriter.error());
                    return;
                }
                updateRequest = request;
                renderer->stop();
                startAnimation();
                // A client that leaves mid-upload never reaches the completion
                // handler; without this the update would stay owned by a dead
                // request and every later upload would get a 409.
                request->onDisconnect([this, request]()
                                      {
                    if (updateRequest == request)
                    {
                        ESP_LOGW(__func__, "Firmware upload disconnected, aborting");
                        cardWriter.abort();
                        updateRequest = nullptr;
                        abandon();
                        firmwareUploadStatus.setFeedback("Upload interrupted - firmware unchanged", dash::Status::WARNING);
                        dashboard->sendUpdates();
                    } });
                firmwareUploadStatus.setFeedback("Receiving firmware...", dash::Status::INFO);
                dashboard->sendUpdates();
            }
            if (updateRequest != request)
            {
                return; // rejected above, or another upload owns Update
            }
            if (len > 0 && !cardWriter.write(data, len))
            {
                ESP_LOGE(__func__, "OTA write failed: %s", cardWriter.error());
                firmwareUploadStatus.setFeedback(cardWriter.error(), dash::Status::DANGER);
                dashboard->sendUpdates();
                cardWriter.abort();
                updateRequest = nullptr;
                abandon();
                request->send(400, "text/plain", Update.errorString());
                return;
            }
            // contentLength() counts the multipart framing too, so this runs
            // a little short of 100% until the end.
            if (request->contentLength() > 0)
            {
                setProgress(float(index + len) / request->contentLength());
            }
            if (final)
            {
                ESP_LOGI(__func__, "Firmware upload received: %u bytes", (unsigned)(index + len));
            }
        });
}

// Task to check for updates
void Updates::checkForUpdates()
{
    for (;;)
    {
        String firmwareUrl;
        String tag;
        if (findFirmwareRelease(tag, firmwareUrl))
        {
            ESP_LOGI(__func__, "Updating %s -> %s from %s", FW_VERSION, tag.c_str(), firmwareUrl.c_str());
            if (downloadAndInstall(firmwareUrl))
            {
                ESP.restart();
            }
        }
        vTaskDelay((CHECK_FOR_UPDATES_INTERVAL * 1000) / portTICK_PERIOD_MS);
    }
}

/**
 * Downloads a release image and installs it, streaming it into OtaWriter.
 * The first bytes are checked (firmware_image::check) before anything is
 * written, the update screen runs while it downloads, and on success the
 * panels fade out. Returns false -- with the pattern back -- on any failure.
 */
bool Updates::downloadAndInstall(const String &url)
{
    NetworkClientSecure client;
    client.setCACertBundle(rootca_crt_bundle_start, rootca_crt_bundle_end - rootca_crt_bundle_start);
    HTTPClient http;
    // browser_download_url redirects to the release asset CDN.
    http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    if (!http.begin(client, url))
    {
        return false;
    }
    const int code = http.GET();
    if (code != HTTP_CODE_OK)
    {
        ESP_LOGW("Updates", "Download failed: HTTP %d", code);
        http.end();
        return false;
    }
    const int total = http.getSize(); // -1 if the server did not say
    NetworkClient *stream = http.getStreamPtr();

    const size_t BUFFER = 4096;
    uint8_t *buf = static_cast<uint8_t *>(malloc(BUFFER));
    if (!buf)
    {
        http.end();
        return false;
    }
    bool ok = false;
    size_t have = 0;
    uint32_t lastData = millis();
    OtaWriter writer;
    while (http.connected() && (total < 0 || writer.written() + have < size_t(total)))
    {
        const size_t room = BUFFER - have;
        const int avail = stream->available();
        if (avail <= 0)
        {
            if (millis() - lastData > 15000)
            {
                ESP_LOGW("Updates", "Download stalled");
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(2));
            continue;
        }
        have += stream->readBytes(buf + have, min(room, size_t(avail)));
        lastData = millis();
        if (!writer.active())
        {
            // Hold off writing until the image can be checked.
            if (have < firmware_image::CHECK_BYTES)
            {
                continue;
            }
            const char *problem = firmware_image::check(buf, have, expectedImage());
            if (problem)
            {
                ESP_LOGE("Updates", "Rejecting release image: %s", problem);
                break;
            }
            if (!writer.begin())
            {
                ESP_LOGE("Updates", "OTA begin failed: %s", writer.error());
                break;
            }
            renderer->stop();
            startAnimation();
        }
        if (have == BUFFER || (total >= 0 && writer.written() + have >= size_t(total)))
        {
            if (!writer.write(buf, have))
            {
                ESP_LOGE("Updates", "OTA write failed: %s", writer.error());
                break;
            }
            have = 0;
            if (total > 0)
            {
                setProgress(float(writer.written()) / total);
            }
        }
    }
    if (writer.active() && have > 0)
    {
        writer.write(buf, have); // the tail, when the size was not known
    }
    free(buf);
    http.end();

    if (writer.active() && (total < 0 || writer.written() == size_t(total)) && writer.finish())
    {
        setProgress(1.0f);
        fadeOut();
        ok = true;
    }
    else
    {
        if (writer.active())
        {
            writer.abort();
        }
        ESP_LOGE("Updates", "GitHub update failed (%u of %d bytes): %s", (unsigned)writer.written(), total, writer.error());
        abandon();
    }
    return ok;
}

/**
 * Finds the release the cube should be running and, if it is not the running
 * one, the URL of its application image.
 *
 * Releases are published by elliotmatson/pio-actions, which names the app
 * image `<env>-<tag>.bin`. That name carries the version, so the old
 * `/releases/latest/download/esp32s3.bin` shortcut cannot be used: the
 * release is looked up through the API and its asset matched by name.
 *
 * Stable cubes follow `/releases/latest`, which GitHub resolves to the newest
 * non-prerelease. Development cubes take the newest release of either kind.
 *
 * @param tag Set to the chosen release's tag.
 * @param firmwareUrl Set to the download URL of the image to install.
 * @return true only when there is an image to install. A tag that matches
 * FW_VERSION exactly, a local "DEV" build, or any failure along the way
 * returns false.
 */
bool Updates::findFirmwareRelease(String &tag, String &firmwareUrl)
{
    // A build that did not come from CI has no version to compare, and every
    // release would look newer than it. Overwriting it a minute after it was
    // flashed is never what was wanted.
    if (strcmp(FW_VERSION, "DEV") == 0)
    {
        ESP_LOGI(__func__, "Local build, not checking GitHub for updates");
        return false;
    }

    const bool development = settings->development();
    String apiUrl = String("https://api.github.com/repos/") + REPO_URL +
                    (development ? "/releases?per_page=10" : "/releases/latest");
    ESP_LOGI(__func__, "Checking %s", apiUrl.c_str());

    NetworkClientSecure client;
    client.setCACertBundle(rootca_crt_bundle_start, rootca_crt_bundle_end - rootca_crt_bundle_start);
    HTTPClient http;
    http.useHTTP10(true); // no chunked encoding, so the body can be streamed into the parser
    if (!http.begin(client, apiUrl))
    {
        ESP_LOGE(__func__, "Could not start request");
        return false;
    }
    http.addHeader("Accept", "application/vnd.github+json");
    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK)
    {
        // 404 from /releases/latest just means nothing has been published yet.
        ESP_LOGW(__func__, "GitHub returned %d", httpCode);
        http.end();
        return false;
    }

    // Only what is used below; a release's JSON is several KB per asset otherwise.
    JsonDocument filter(spiRamAllocator());
    JsonObject releaseFilter = development ? filter[0].to<JsonObject>() : filter.to<JsonObject>();
    releaseFilter["tag_name"] = true;
    releaseFilter["draft"] = true;
    releaseFilter["published_at"] = true;
    releaseFilter["assets"][0]["name"] = true;
    releaseFilter["assets"][0]["browser_download_url"] = true;

    JsonDocument doc(spiRamAllocator());
    DeserializationError err = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
    http.end();
    if (err)
    {
        ESP_LOGE(__func__, "Could not parse release list: %s", err.c_str());
        return false;
    }

    JsonObject release;
    if (development)
    {
        // ISO 8601 timestamps in the same zone compare correctly as strings.
        const char *newest = "";
        for (JsonObject candidate : doc.as<JsonArray>())
        {
            const char *published = candidate["published_at"] | "";
            if (!(candidate["draft"] | false) && strcmp(published, newest) > 0)
            {
                release = candidate;
                newest = published;
            }
        }
    }
    else
    {
        release = doc.as<JsonObject>();
    }
    if (release.isNull() || !release["tag_name"].is<const char *>())
    {
        ESP_LOGW(__func__, "No release found");
        return false;
    }

    tag = release["tag_name"].as<const char *>();
    // Exact match: a substring test would treat v0.2.1 as already running v0.2.10.
    if (tag == FW_VERSION)
    {
        ESP_LOGI(__func__, "Already running %s", FW_VERSION);
        return false;
    }

    String assetName = String(FW_ENV) + "-" + tag + ".bin";
    for (JsonObject asset : release["assets"].as<JsonArray>())
    {
        if (assetName == (asset["name"] | ""))
        {
            firmwareUrl = asset["browser_download_url"].as<const char *>();
            return firmwareUrl.length() > 0;
        }
    }
    ESP_LOGW(__func__, "Release %s has no %s", tag.c_str(), assetName.c_str());
    return false;
}

// Task to handle OTA updates
void Updates::checkForOTA()
{
    for (;;)
    {
        ArduinoOTA.handle();
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

