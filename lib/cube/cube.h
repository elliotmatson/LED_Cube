#ifndef CUBE_H
#define CUBE_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <ArduinoOTA.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <FastLED.h>
#include <Preferences.h>
#include <WiFiManager.h>
//#include <WebSerial.h>

#include "config.h"

// MACROS
// Calculates Cosine quickly using constants in flash
#define FAST_COS(x) (cos_wave[(uint16_t)(x) % 256])
// Calculates precise projected X values of a pixel
#define PROJ_CALC_X(x, y) ((x < 64) ? (0.8660254 * (x - y)) : ((x < 128) ? (111.7173 - 0.8660254 * x) : (109.1192 - 0.8660254 * x)))
// Calculates less precise, but faster projected X values of a pixel
#define PROJ_CALC_INT_X(x, y) ((x < 64) ? ((7 * (x - y)) >> 3) : ((x < 128) ? (112 - ((7 * x) >> 3)) : (109 - ((7 * x) >> 3))))
// Calculates precise projected Y values of a pixel
#define PROJ_CALC_Y(x, y) ((x < 64) ? ((130 - x - y) >> 1) : ((x < 128) ? (y - (x >> 1)) : ((x >> 1) + y - 128)))

extern uint8_t const cos_wave[256];
extern TaskHandle_t checkForUpdatesTask;
extern TaskHandle_t checkForOTATask;
extern MatrixPanel_I2S_DMA *dma_display;
extern Preferences prefs;

void initPrefs();
void initUpdates();
void initDisplay();
void initWifi();
void showDebug();
void firmwareUpdate();
void checkForUpdates(void *parameter);
void checkForOTA(void *parameter);
void printMem();

#endif