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

#include "config.h"
#include "pattern.h"
#include "all_patterns.h"

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

    private:
        TaskHandle_t checkForUpdatesTask;
        TaskHandle_t checkForOTATask;
        TaskHandle_t showPatternTask;
        Preferences prefs;
        uint8_t brightness;
        void initPrefs();
        void initUpdates();
        void initDisplay();
        void initWifi();
        patterns currentPattern;

};
#ifndef DEVELOPMENT
void checkForUpdates(void *parameter);
#endif
void checkForOTA(void *parameter);
void showPattern(void *parameter);

#endif