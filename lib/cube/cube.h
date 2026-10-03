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
#include "settings.h"
#include "renderer.h"
#include "updates.h"
#include "timezones.h"
#include "cube_utils.h"
#include "all_patterns.h"

#if __has_include("secrets.h")
#include "secrets.h"
#endif

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
    Settings settings;
    Renderer renderer;
    std::unordered_map<std::string, Pattern *> patterns;
    std::vector<std::string> patternButtonLabels;

    // Variables
    String serial;
    bool wifiReady;

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
    dash::DropdownCard<> timezoneDropdown;
    Updates updates; // owns the firmware upload cards
    dash::Tab systemTab;
    dash::Tab developerTab;

    // FreeRTOS Tasks
    TaskHandle_t printMemTask = nullptr;

    // Functions
    void showDebug();
    void showCoordinates();
    void showTestSequence();
    void setBrightness(uint8_t brightness);
    uint8_t getBrightness();
    void setDevelopment(bool development);
    void setSignedFWOnly(bool signedFWOnly);
    bool initDisplay();
    bool initWifi();
    void initUI();
    void initAPI();
    void printMem();
    const timezones::Zone &currentTimezone();
};

#endif