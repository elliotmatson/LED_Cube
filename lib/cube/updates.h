#ifndef UPDATES_H
#define UPDATES_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <ESPAsyncWebServer.h>
#include <ESPDashPro.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <NetworkClientSecure.h>
#include <Update.h>
#include <esp_ota_ops.h>

#include "config.h"
#include "firmware_image.h"
#include "renderer.h"
#include "settings.h"

// get ESP-IDF Certificate Bundle
extern const uint8_t rootca_crt_bundle_start[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t rootca_crt_bundle_end[] asm("_binary_x509_crt_bundle_end");

// The cube's custom app descriptor (see updates.cpp).
struct CubePartition
{
    char cookie[32];
    char reserved[224]; // Reserved for future use, total of 256 bytes
};

/**
 * Writes an app image to the next OTA partition through ESP-IDF's OTA API in
 * sequential-write mode: each 4 KB sector is erased just before it is
 * written. Arduino's Update erases 64 KB blocks instead, and an erase pauses
 * both cores -- about 200 ms per block against about 45 ms per sector -- which
 * is what made the update screen stutter. esp_ota_end() checks the image
 * (chip, segments, hash) before it can be made bootable.
 */
class OtaWriter
{
public:
    bool begin();
    bool write(const uint8_t *data, size_t len);
    /// Verifies the image and makes it the boot partition.
    bool finish();
    void abort();
    bool active() const { return handle != 0; }
    size_t written() const { return bytes; }
    const char *error() const { return esp_err_to_name(err); }

private:
    esp_ota_handle_t handle = 0;
    const esp_partition_t *partition = nullptr;
    esp_err_t err = ESP_OK;
    size_t bytes = 0;
};

/**
 * The three ways firmware reaches the cube, and what they share:
 *
 * - the dashboard's upload card, streamed into the inactive OTA slot and
 *   checked before a byte is written;
 * - ArduinoOTA (`pio run -t upload`), when switched on -- this one still
 *   goes through Arduino's Update, so its update screen stutters more;
 * - the GitHub updater, polling the repository's releases hourly.
 *
 * All three accept only images firmware_image::check() passes. ArduinoOTA and
 * the GitHub updater stop the renderer and draw progress on the panels.
 */
class Updates
{
public:
    /// Creates the upload card and its status card on `dash`.
    explicit Updates(ESPDash &dash);

    void begin(AsyncWebServer &server, MatrixPanel_I2S_DMA *panels, Settings &settings, Renderer &renderer, dash::Tab &tab);

    /// Turns ArduinoOTA on or off and saves the choice. Idempotent.
    void setOta(bool ota);
    /// Turns GitHub updates on or off and saves the choice. Idempotent.
    void setGithub(bool github);

private:
    void initFirmwareUpload();
    firmware_image::Expected expectedImage();
    void verifyWrittenImage();
    // The update screen: rows of "UPDATE" across the side faces, moving one
    // at a time, the percentage on top, and a line round the top face's edge
    // as progress. Drawn on its own canvas while the renderer is stopped.
    void startAnimation();
    void stopAnimation();
    void setProgress(float fraction);
    void drawFrame();
    void drawRow(BottomPanels &strip, int r);
    void drawEdgeProgress(SinglePanel &top, int from, int to);
    void animationLoop();
    /// Stops the animation and brings the pattern back after a failed update.
    void abandon();
    void onProgress(unsigned int progress, unsigned int total);
    void fadeOut();
    void checkForUpdates();
    bool findFirmwareRelease(String &tag, String &firmwareUrl);
    bool downloadAndInstall(const String &url);
    void checkForOTA();

    ESPDash *dashboard;
    AsyncWebServer *server = nullptr;
    MatrixPanel_I2S_DMA *panels = nullptr;
    Settings *settings = nullptr;
    Renderer *renderer = nullptr;

    dash::FileUploadCard<> firmwareUploadCard;
    dash::FeedbackCard<> firmwareUploadStatus;
    // The upload that owns the Update object, or null. Set only after its
    // image passed the checks and Update.begin() succeeded.
    AsyncWebServerRequest *updateRequest = nullptr;
    OtaWriter cardWriter; // the upload card's image in progress

    // The update screen runs in its own task, so drawing it never slows the
    // update: the update paths only record progress (in permille).
    Canvas canvas;
    bool canvasReady = false;
    bool firstFrame = true;
    int16_t rowOffset[4] = {};
    int nextRow = 0;
    int shownPercent = -1;
    int shownEdge = 0;
    volatile uint16_t progressPermille = 0;
    volatile bool animating = false;
    TaskHandle_t animationTask = nullptr;
    SemaphoreHandle_t animationDone = nullptr;

    TaskHandle_t otaTask = nullptr;
    TaskHandle_t githubTask = nullptr;
};

#endif
