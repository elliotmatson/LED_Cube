#ifndef CUBE_H
#define CUBE_H

#include <stdio.h>
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
#include <WiFiClientSecure.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <esp_ota_ops.h>
#include <ESPmDNS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESPDashPro.h>
#include <WebSerial.h>

#include "config.h"
#include "pattern.h"
#include "all_patterns.h"

// get ESP-IDF Certificate Bundle
extern const uint8_t rootca_crt_bundle_start[] asm("_binary_x509_crt_bundle_start");


// for ArduinoJson with SPI RAM
struct SpiRamAllocator
{
    void *allocate(size_t size)
    {
        return heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    }

    void deallocate(void *pointer)
    {
        heap_caps_free(pointer);
    }

    void *reallocate(void *ptr, size_t new_size)
    {
        return heap_caps_realloc(ptr, new_size, MALLOC_CAP_SPIRAM);
    }
};
using SpiRamJsonDocument = BasicJsonDocument<SpiRamAllocator>;

// Preferences struct for storing and loading in nvs
struct CubePrefs
{
    uint8_t brightness = 255;
    bool development = 0;
    bool ota = 0;
    bool github = 1;
    bool signedFWOnly = 1;
    uint8_t latchBlanking = 2;
    bool use20MHz = 0;
    void print(String prefix) {
        Serial.printf("%s\nBrightness: %d\nDevelopment: %d\nOTA: %d\nGithub: %d\nSigned FW Only: %d\n", prefix.c_str(), brightness, development, ota, github, signedFWOnly);
    }
};

// Partition struct for verifying firmware is intended for cube
struct CubePartition
{
    char cookie[32];
    char reserved[224]; // Reserved for future use, total of 256 bytes
};

class Cube {
    public:
        Cube();
        void init();
        MatrixPanel_I2S_DMA *dma_display;
        void printMem();
        void showDebug();
        void showCoordinates();
        void showTestSequence();
        void setBrightness(uint8_t brightness);
        uint8_t getBrightness();
        void printf(const char *format, ...);

    private:
        WiFiManager wifiManager;
        CubePrefs cubePrefs;
        String serial;
        patterns currentPattern;
        bool wifiReady;
        AsyncWebServer server;
        ESPDash dashboard;
        Card otaToggle;
        Card GHUpdateToggle;
        Card developmentToggle;
        Card signedFWOnlyToggle;
        Statistic fwVersion;
        Card brightnessSlider;
        Card latchSlider;
        Card use20MHzToggle;
        Card rebootButton;
        Card resetWifiButton;
        TaskHandle_t checkForUpdatesTask;
        TaskHandle_t checkForOTATask;
        TaskHandle_t showPatternTask;
        Preferences prefs;
        Preferences patternPrefs;

        void setDevelopment(bool development);
        void setOTA(bool ota);
        void setGHUpdate(bool github);
        void setSignedFWOnly(bool signedFWOnly);
        void initPrefs();
        void initUpdates();
        void initDisplay();
        void initWifi();
        void initUI();
        void checkForUpdates();
        void checkForOTA();
        void showPattern();
        void updatePrefs();
};

#endif