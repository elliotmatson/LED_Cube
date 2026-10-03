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
 * The three ways firmware reaches the cube, and what they share:
 *
 * - the dashboard's upload card, streamed into the inactive OTA slot and
 *   checked before a byte is written;
 * - ArduinoOTA (`pio run -t upload`), when switched on;
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
    // The update screen: "UPDATE" tiled and scrolling a different way on each
    // face, the percentage on top, and a line tracing the cube's edges as
    // progress. Drawn on its own canvas while the renderer is stopped.
    void startAnimation();
    void stopAnimation();
    void setProgress(float fraction);
    void drawFrame();
    void drawEdgeProgress(int steps);
    void animationLoop();
    /// Stops the animation and brings the pattern back after a failed update.
    void abandon();
    void onProgress(unsigned int progress, unsigned int total);
    void fadeOut();
    void checkForUpdates();
    bool findFirmwareRelease(String &tag, String &firmwareUrl);
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

    // The update screen runs in its own task, so drawing it never slows the
    // update: the update paths only record progress (in permille).
    Canvas canvas;
    bool canvasReady = false;
    uint32_t animationStartMs = 0;
    volatile uint16_t progressPermille = 0;
    volatile bool animating = false;
    TaskHandle_t animationTask = nullptr;
    SemaphoreHandle_t animationDone = nullptr;

    TaskHandle_t otaTask = nullptr;
    TaskHandle_t githubTask = nullptr;
};

#endif
