#include "updates.h"

#include <esp_app_desc.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

// The cube's custom app descriptor, placed right after the standard one in
// every image (ESP-IDF's .rodata_custom_desc). Its cookie is the "signature"
// checked when "Signed FW only" is on. It is a fixed string, not a
// cryptographic signature: it tells cube firmware from other firmware, and
// does not stop anyone determined.
const __attribute__((section(".rodata_custom_desc"))) CubePartition cubePartition = {CUBE_MAGIC_COOKIE};

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

/// A word on all three faces while an update runs.
void Updates::showBanner(const char *text)
{
    panels->fillScreenRGB888(0, 0, 0);
    panels->setFont(NULL);
    panels->setCursor(6, 21);
    panels->setTextColor(0xFFFF);
    panels->setTextSize(3);
    panels->print(text);
}

/// Progress as a line tracing the edges of all three faces.
void Updates::drawProgress(unsigned int progress, unsigned int total)
{
    ESP_LOGI("Updates", "Progress: %u%%", total ? (progress * 100) / total : 0);
    if (settings->signedFirmwareOnly() && progress == total)
    {
        verifyWrittenImage();
    }

    int i = map(progress, 0, total, 0, 512);
    panels->drawFastHLine(128, 0, constrain(i, 0, 64), 0xFFFF);
    panels->drawFastVLine(191, 0, constrain(i - 64, 0, 64), 0xFFFF);

    panels->drawFastVLine(0, 64 - constrain(i - 128, 0, 63), constrain(i - 128, 0, 64), 0xFFFF);
    panels->drawFastHLine(0, 0, constrain(i - 192, 0, 64), 0xFFFF);

    panels->drawFastVLine(64, 64 - constrain(i - 256, 0, 63), constrain(i - 256, 0, 64), 0xFFFF);
    panels->drawFastHLine(64, 0, constrain(i - 320, 0, 64), 0xFFFF);

    panels->drawFastVLine(127, 0, constrain(i - 384, 0, 64), 0xFFFF);
    panels->drawFastVLine(128, 0, constrain(i - 384, 0, 64), 0xFFFF);

    panels->drawFastHLine(128 - constrain(i - 448, 0, 63), 63, constrain(i - 448, 0, 64), 0xFFFF);
    panels->drawFastHLine(128, 63, constrain(i - 448, 0, 64), 0xFFFF);
    panels->drawFastHLine(64 - constrain(i - 448, 0, 64), 63, constrain(i - 448, 0, 64), 0xFFFF);
    panels->drawFastVLine(63, 64 - constrain(i - 448, 0, 64), constrain(i - 448, 0, 64), 0xFFFF);
}

/// Dims the panels to black before the restart into new firmware.
void Updates::fadeOut()
{
    settings->flush();
    for (int i = settings->brightness(); i > 0; i -= 3)
    {
        panels->setBrightness8(max(i, 0));
    }
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
                showBanner("OTA"); })
            .onEnd([this]()
                   { fadeOut(); })
            .onProgress([this](unsigned int progress, unsigned int total)
                        { drawProgress(progress, total); })
            .onError([this](ota_error_t error)
                     {
                ESP_LOGE("Updates", "ArduinoOTA error %u", error);
                // The pattern was stopped in onStart; nothing will reboot
                // into new firmware now, so bring it back.
                renderer->resume(); });
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
        httpUpdate.onStart([this]()
                           {
            ESP_LOGI("Updates", "GitHub update starting");
            renderer->stop();
            showBanner("GHA"); });
        httpUpdate.onEnd([this]()
                         { fadeOut(); });
        httpUpdate.onProgress([this](unsigned int progress, unsigned int total)
                              { drawProgress(progress, total); });
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
 * it once Update.end() has checked it and marked it bootable.
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

            // end(true) verifies the image and sets the boot partition.
            if (!Update.end(true))
            {
                ESP_LOGE(__func__, "Update.end failed: %s", Update.errorString());
                firmwareUploadStatus.setFeedback(Update.errorString(), dash::Status::DANGER);
                dashboard->sendUpdates();
                request->send(400, "text/plain", Update.errorString());
                return;
            }

            ESP_LOGI(__func__, "Firmware update staged, restarting");
            firmwareUploadStatus.setFeedback("Update complete - restarting", dash::Status::SUCCESS);
            dashboard->sendUpdates();
            AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "Update complete, restarting");
            response->addHeader("Connection", "close");
            request->send(response);
            settings->flush();
            // Long enough for the response and the dashboard update to go out.
            xTaskCreate(
                [](void *)
                {
                    vTaskDelay(pdMS_TO_TICKS(1500));
                    ESP.restart();
                },
                "Restart", 2048, nullptr, 1, nullptr);
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
                // UPDATE_SIZE_UNKNOWN: contentLength() includes the multipart
                // framing and overstates the image.
                if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH))
                {
                    ESP_LOGE(__func__, "Update.begin failed: %s", Update.errorString());
                    request->send(500, "text/plain", Update.errorString());
                    return;
                }
                updateRequest = request;
                // A client that leaves mid-upload never reaches the completion
                // handler; without this the update would stay owned by a dead
                // request and every later upload would get a 409.
                request->onDisconnect([this, request]()
                                      {
                    if (updateRequest == request)
                    {
                        ESP_LOGW(__func__, "Firmware upload disconnected, aborting");
                        Update.abort();
                        updateRequest = nullptr;
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
            if (len > 0 && Update.write(data, len) != len)
            {
                ESP_LOGE(__func__, "Update.write failed: %s", Update.errorString());
                firmwareUploadStatus.setFeedback(Update.errorString(), dash::Status::DANGER);
                dashboard->sendUpdates();
                Update.abort();
                updateRequest = nullptr;
                request->send(400, "text/plain", Update.errorString());
                return;
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
            NetworkClientSecure client;
            client.setCACertBundle(rootca_crt_bundle_start, rootca_crt_bundle_end - rootca_crt_bundle_start);
            // browser_download_url redirects to the release asset CDN.
            httpUpdate.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
            t_httpUpdate_return ret = httpUpdate.update(client, firmwareUrl);

            switch (ret)
            {
            case HTTP_UPDATE_FAILED:
                ESP_LOGE(__func__, "Http Update Failed (Error=%d): %s", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
                // onStart may already have stopped the pattern.
                renderer->resume();
                break;

            case HTTP_UPDATE_NO_UPDATES:
                ESP_LOGI(__func__, "No Update!");
                break;

            case HTTP_UPDATE_OK:
                ESP_LOGI(__func__, "Update OK!");
                break;
            }
        }
        vTaskDelay((CHECK_FOR_UPDATES_INTERVAL * 1000) / portTICK_PERIOD_MS);
    }
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

