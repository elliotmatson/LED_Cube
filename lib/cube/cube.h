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

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESPDash.h>
#include <WebSerial.h>

#include "config.h"
#include "pattern.h"
#include "all_patterns.h"

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


class Cube {
    public:
        Cube(bool ota = 0, bool github = 1, bool development = 0);
        void init();
        MatrixPanel_I2S_DMA *dma_display;
        void printMem();
        void showDebug();
        void showCoordinates();
        void showTestSequence();
        void setBrightness(uint8_t brightness);
        uint8_t getBrightness();
        void setDevelopment(bool development);
        void setOTA(bool ota);
        void setGHUpdate(bool github);

    private:
        TaskHandle_t checkForUpdatesTask;
        TaskHandle_t checkForOTATask;
        TaskHandle_t showPatternTask;
        Preferences prefs;
        uint8_t brightness;
        String serial;
        bool developmentBranch;
        bool otaEnabled;
        bool GHUpdateEnabled;
        void initPrefs();
        void initUpdates();
        void initDisplay();
        void initWifi();
        void checkForUpdates();
        void checkForOTA();

        patterns currentPattern;
        AsyncWebServer *server;
        ESPDash *dashboard;
        Card *otaToggle;
        Card *GHUpdateToggle;
        Card *developmentToggle;
        Statistic *fwVersion;
};

void showPattern(void *parameter);

#endif