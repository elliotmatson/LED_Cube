#ifndef CUBE_H
#define CUBE_H

#include <sdkconfig.h>
#include <stdio.h>
#include <vector>
#include <Arduino.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <ArduinoOTA.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <Preferences.h>
#include <WiFiManager.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <WiFiClient.h>
#include <NetworkClientSecure.h>
#include <WiFi.h>
#include <esp_ota_ops.h>
#include <ESPmDNS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESPDashPro.h>
#include <Update.h>
#include <esp_app_desc.h>
#include <Adafruit_NeoPixel.h>
#include "time.h"
#include "AsyncJson.h"
#include <ArduinoJson.h>
#include "esp_ota_ops.h"
#include "esp_app_format.h"
#include "esp_flash_partitions.h"
#include "esp_partition.h"

#include "config.h"
#include "cube_utils.h"
#include "all_patterns.h"

#if __has_include("secrets.h")
#include "secrets.h"
#endif

// get ESP-IDF Certificate Bundle
extern const uint8_t rootca_crt_bundle_start[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t rootca_crt_bundle_end[] asm("_binary_x509_crt_bundle_end");

// Preferences struct for storing and loading in nvs
struct CubePrefs
{
    uint8_t brightness = 255;
    bool development = 0;
    bool ota = 0;
    bool github = 1;
    bool signedFWOnly = 1;
    uint8_t latchBlanking = 1;
    bool use20MHz = 0;
    u_int8_t patternIndex = 0;
    void print(String prefix)
    {
        ESP_LOGI(__func__, "%s\nBrightness: %d\nDevelopment: %d\nOTA: %d\nGithub: %d\nSigned FW Only: %d\n", prefix.c_str(), brightness, development, ota, github, signedFWOnly);
    }
};

// Partition struct for verifying firmware is intended for cube
struct CubePartition
{
    char cookie[32];
    char reserved[224]; // Reserved for future use, total of 256 bytes
};

class Cube
{
public:
    Cube();
    void init();

private:
    // Objects
    MatrixPanel_I2S_DMA *dma_display;
    Adafruit_NeoPixel leds;
    AsyncWebServer server;
    WiFiManager wifiManager;
    CubePrefs cubePrefs;
    Pattern *currentPattern = nullptr;
    bool patternRunning = false;
    // Held for the whole of a pattern's stop() or init()/start(). Switches
    // come from the dashboard, the API, the boot path and three update paths,
    // each in a different task.
    SemaphoreHandle_t patternMutex = nullptr;
    // Dashboard handlers run in the AsyncTCP task and must not block it, so
    // they only post the index of the pattern they want here. Length 1: a
    // newer request replaces one not yet acted on.
    QueueHandle_t patternRequests = nullptr;
    PatternServices patternServices;
    std::unordered_map<std::string, Pattern *> patterns;
    std::vector<std::string> patternButtonLabels;

    // Variables
    String serial;
    bool wifiReady;
    // The upload that owns the Update object, or null. Set only after its
    // image passed checkFirmwareImage() and Update.begin() succeeded.
    AsyncWebServerRequest *updateRequest = nullptr;

    // UI Components
    ESPDash dashboard;
    dash::ToggleButtonCard otaToggle;
    dash::ToggleButtonCard GHUpdateToggle;
    dash::ToggleButtonCard developmentToggle;
    dash::ToggleButtonCard signedFWOnlyToggle;
    dash::StatisticValue<dash::string> fwVersion;
    dash::SliderCard<int> brightnessSlider;
    dash::SliderCard<int> latchSlider;
    dash::ToggleButtonCard use20MHzToggle;
    dash::PushButtonCard rebootButton;
    dash::PushButtonCard resetWifiButton;
    dash::PushButtonCard crashMe;
    dash::FileUploadCard<> firmwareUploadCard;
    dash::FeedbackCard<> firmwareUploadStatus;
    dash::Tab systemTab;
    dash::Tab developerTab;

    // FreeRTOS Tasks
    TaskHandle_t checkForUpdatesTask = nullptr;
    TaskHandle_t checkForOTATask = nullptr;
    TaskHandle_t printMemTask = nullptr;
    TaskHandle_t patternTask = nullptr;
    Preferences prefs;

    // Functions
    void showDebug();
    void showCoordinates();
    void showTestSequence();
    void setBrightness(uint8_t brightness);
    uint8_t getBrightness();
    void setDevelopment(bool development);
    void setOTA(bool ota);
    void setGHUpdate(bool github);
    void setSignedFWOnly(bool signedFWOnly);
    bool initPrefs();
    void initUpdates();
    bool initDisplay();
    bool initWifi();
    void initUI();
    void initAPI();
    void initFirmwareUpload();
    void checkForUpdates();
    bool findFirmwareRelease(String &tag, String &firmwareUrl);
    void checkForOTA();
    void updatePrefs();
    void printMem();
    void startPattern(Pattern *pattern);
    void stopPattern();
    void resumePattern();
    void requestPattern(size_t index);
    void patternWorker();
};

#endif